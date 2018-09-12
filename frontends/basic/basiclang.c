/*
 * Spin to C/C++ converter
 * Copyright 2011-2018 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for BASIC specific features
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

static AST *basic_print_float;
static AST *basic_print_string;
static AST *basic_print_integer;
static AST *basic_print_unsigned;
static AST *basic_print_char;
static AST *basic_print_nl;

static AST *float_add;
static AST *float_sub;
static AST *float_mul;
static AST *float_div;

static AST *
addPrintCall(AST *seq, AST *func, AST *expr)
{
    AST *elem;
    AST *funccall = NewAST(AST_FUNCCALL, func,
                           expr ? NewAST(AST_EXPRLIST, expr, NULL) : NULL);
    elem = NewAST(AST_SEQUENCE, funccall, NULL);
    return AddToList(seq, elem);
}

static void
doBasicTransform(AST **astptr)
{
    AST *ast = *astptr;
    
    while (ast && ast->kind == AST_COMMENTEDNODE) {
        astptr = &ast->left;
        ast = *astptr;
    }
    if (!ast) return;
    AstReportAs(ast); // any newly created AST nodes should reflect debug info from this one
    switch (ast->kind) {
    case AST_ASSIGN:
        if (ast->left && ast->left->kind == AST_RANGEREF) {
            *astptr = ast = TransformRangeAssign(ast->left, ast->right, 1);
        }
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        break;
    case AST_COUNTREPEAT:
        // convert repeat count into a for loop
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        *astptr = TransformCountRepeat(*astptr);
        break;
    case AST_RANGEREF:
        *astptr = ast = TransformRangeUse(ast);
        break;
    case AST_FUNCCALL:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        if (1)
        {
            // the parser treats a(x) as a function call (always), but in
            // fact it may be an array reference; change it to one if applicable
            AST *left = ast->left;
            AST *index = ast->right;
            AST *typ;
        
            typ = ExprType(left);
            if (typ && (IsPointerType(typ) || IsArrayType(typ))) {
                ast->kind = AST_ARRAYREF;
                if (!index || index->kind != AST_EXPRLIST) {
                    ERROR(ast, "Internal error: expected expression list in array subscript");
                    return;
                }
                // reduce a single item expression list, if necessary
                if (index->right != NULL) {
                    ERROR(index, "Multi-dimensional arrays are not supported\n");
                    return;
                }
                index = index->left;
                // and now we have to convert the array reference to
                // BASIC style (subtract one)
                index = AstOperator('-', index, AstInteger(1));
                ast->right = index;
            }
        }
        break;
    case AST_PRINT:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        {
            // convert PRINT to a series of calls to basic_print_xxx
            AST *seq = NULL;
            AST *type;
            AST *exprlist = ast->left;
            AST *expr;
            while (exprlist) {
                expr = exprlist->left;
                exprlist = exprlist->right;
                if (expr->kind == AST_CHAR) {
                    expr = expr->left;
                    if (IsConstExpr(expr) && EvalConstExpr(expr) == 10) {
                        seq = addPrintCall(seq, basic_print_nl, NULL);
                    } else {
                        seq = addPrintCall(seq, basic_print_char, expr);
                    }
                    continue;
                }
                type = ExprType(expr);
                if (!type) {
                    ERROR(ast, "Unknown type in print");
                    continue;
                }
                if (type == ast_type_float) {
                    seq = addPrintCall(seq, basic_print_float, expr);
                } else if (type == ast_type_string) {
                    seq = addPrintCall(seq, basic_print_string, expr);
                } else if (type->kind == AST_INTTYPE) {
                    seq = addPrintCall(seq, basic_print_integer, expr);
                } else if (type->kind == AST_UNSIGNEDTYPE) {
                    seq = addPrintCall(seq, basic_print_unsigned, expr);
                } else {
                    ERROR(ast, "Unable to print expression of this type");
                }
            }
            *astptr = ast = seq;
        }
        break;
    default:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        break;
    }
}

bool VerifyIntegerType(AST *astForError, AST *typ, const char *opname)
{
    if (!typ)
        return true;
    if (IsIntType(typ))
        return true;
    ERROR(astForError, "Expected integer type for parameter of %s", opname);
    return false;
}

bool IsUnsignedType(AST *typ) {
    return typ && typ->kind == AST_UNSIGNEDTYPE;
}

static AST *dopromote(AST *expr, int numbytes, int operator)
{
    int shiftbits = numbytes * 8;
    AST *promote = AstOperator(operator, expr, AstInteger(shiftbits));
    return promote;
}

//
// insert promotion code under AST for either the left or right type
// if "force" is nonzero then we will always promote small integers,
// otherwise we promote only if their sizes do not match
// return the final type
//
AST *MatchIntegerTypes(AST *ast, AST *lefttype, AST *righttype, int force) {
    int lsize = TypeSize(lefttype);
    int rsize = TypeSize(righttype);
    AST *rettype = lefttype;
    int leftunsigned = IsUnsignedType(lefttype);
    int rightunsigned = IsUnsignedType(righttype);
    
    force = force || (lsize != rsize);
    
    if (lsize < 4 && force) {
        if (IsConstExpr(ast->right)) {
            return lefttype;
        }
        if (leftunsigned) {
            ast->left = dopromote(ast->left, lsize, K_ZEROEXTEND);
            lefttype = ast_type_unsigned_long;
        } else {
            ast->left = dopromote(ast->left, lsize, K_SIGNEXTEND);
            lefttype = ast_type_long;
        }
        rettype = righttype;
    }
    if (rsize < 4 && force) {
        if (IsConstExpr(ast->left)) {
            return righttype;
        }
        if (rightunsigned) {
            ast->right = dopromote(ast->right, rsize, K_ZEROEXTEND);
            righttype = ast_type_unsigned_long;
        } else {
            ast->right = dopromote(ast->right, rsize, K_SIGNEXTEND);
            righttype = ast_type_long;
        }
        rettype = lefttype;
    }
    if (leftunsigned && rightunsigned) {
        return rettype;
    } else {
        return ast_type_long;
    }
}

static AST *
domakefloat(AST *ast)
{
    AST *ret;
    if (!ast) return ast;
    if (gl_fixedreal) {
        ret = AstOperator(K_SHL, ast, AstInteger(16));
        return ret;
    }
    ERROR(ast, "Cannot handle float expressions yet");
    return ast;
}

static AST *
dofloatToInt(AST *ast)
{
    AST *ret;
    if (gl_fixedreal) {
        // FIXME: should we round here??
        ret = AstOperator(K_SAR, ast, AstInteger(16));
        return ret;
    }
    ERROR(ast, "Cannot handle float expressions yet");
    return ast;
}

bool BothIntegers(AST *ast, AST *ltyp, AST *rtyp, const char *opname)
{
    if (IsFloatType(ltyp)) {
        ast->left = dofloatToInt(ast->left);
        ltyp = ast_type_long;
    }
    if (IsFloatType(rtyp)) {
        ast->right = dofloatToInt(ast->right);
        rtyp = ast_type_long;
    }
    return VerifyIntegerType(ast, ltyp, opname) && VerifyIntegerType(ast, rtyp, opname);
}

// call function func with parameters ast->left, ast->right
static void
NewOperatorCall(AST *ast, AST *func)
{
    AST *call;
    AST *params = NULL;

    if (ast->left) {
        params = AddToList(params, NewAST(AST_EXPRLIST, ast->left, NULL));
    }
    if (ast->right) {
        params = AddToList(params, NewAST(AST_EXPRLIST, ast->right, NULL));
    }
    call = NewAST(AST_FUNCCALL, func, params);
    *ast = *call;
}

static AST *
HandleTwoNumerics(int op, AST *ast, AST *lefttype, AST *righttype)
{
    int isfloat = 0;
    if (IsFloatType(lefttype)) {
        if (!IsFloatType(righttype)) {
            ast->right = domakefloat(ast->right);
        }
        isfloat = 1;
    } else if (IsFloatType(righttype)) {
        ast->left = domakefloat(ast->left);
        isfloat = 1;
    }
    if (isfloat) {
        // FIXME need to call appropriate funcs here
        switch (op) {
        case '+':
            if (!gl_fixedreal) {
                NewOperatorCall(ast, float_add);
            }
            break;
        case '-':
            if (!gl_fixedreal) {
                NewOperatorCall(ast, float_sub);
            }
            break;
        case '*':
            NewOperatorCall(ast, float_mul);
            break;
        case '/':
            NewOperatorCall(ast, float_div);
            break;
        default:
            ERROR(ast, "internal error unhandled operator");
            break;
        }
        return ast_type_float;
    }
    if (!BothIntegers(ast, lefttype, righttype, "operator"))
        return NULL;
    return MatchIntegerTypes(ast, lefttype, righttype, 0);
}

void CompileComparison(int op, AST *ast, AST *lefttype, AST *righttype)
{
    int isfloat = 0;
    
    if (IsFloatType(lefttype)) {
        if (!IsFloatType(righttype)) {
            ast->right = domakefloat(ast->right);
        }
        isfloat = 1;
    } else if (IsFloatType(righttype)) {
        ast->left = domakefloat(ast->left);
        isfloat = 1;
    }
    if (isfloat) {
        if (gl_fixedreal) {
            // we're good
        } else {
            ERROR(ast, "Internal error with floats");
        }
        return;
    }
    if (!BothIntegers(ast, lefttype, righttype, "comparison")) {
        return;
    }
    // should handle unsigned/signed comparisons here
}

AST *CoerceOperatorTypes(AST *ast, AST *lefttype, AST *righttype)
{
    //assert(ast->kind == AST_OPERATOR)
    if (!ast->left) {
        return righttype;
    }
    if (!ast->right) {
        return righttype;
    }
    switch(ast->d.ival) {
    case K_SAR:
    case K_SHL:
    case '&':
    case '|':
    case '^':
        if (lefttype && IsFloatType(lefttype)) {
            ast->left = dofloatToInt(ast->left);
            lefttype = ast_type_long;
        }
        if (righttype && IsFloatType(righttype)) {
            ast->right = dofloatToInt(ast->right);
            righttype = ast_type_long;
        }
        if (!BothIntegers(ast, lefttype, righttype, "bit operation")) {
            return NULL;
        }
        if (ast->d.ival == K_SAR && lefttype && IsUnsignedType(lefttype)) {
            ast->d.ival = K_SHR;
        }
        return lefttype;
    case '+':
    case '-':
    case '*':
    case '/':
        return HandleTwoNumerics(ast->d.ival, ast, lefttype, righttype);
    case K_SIGNEXTEND:
        VerifyIntegerType(ast, righttype, "sign extension");
        return ast_type_long;
    case K_ZEROEXTEND:
        VerifyIntegerType(ast, righttype, "zero extension");
        return ast_type_unsigned_long;
    case '<':
    case K_LE:
    case K_EQ:
    case K_NE:
    case K_GE:
    case '>':
        CompileComparison(ast->d.ival, ast, lefttype, righttype);
        return ast_type_long;
    default:
        if (!BothIntegers(ast, lefttype, righttype, "operator")) {
            return NULL;
        }
        return MatchIntegerTypes(ast, lefttype, righttype, 1);
    }
}

AST *CoerceAssignTypes(AST *ast, AST *desttype, AST *srctype)
{
    if (!desttype || !srctype || IsGenericType(desttype) || IsGenericType(srctype)) {
        return desttype;
    }
    if (!CompatibleTypes(desttype, srctype)) {
        ERROR(ast, "incompatible types in assignment");
        return desttype;
    }
    if (IsConstType(desttype)) {
        WARNING(ast, "assignment to const object");
    }
    if (IsPointerType(srctype) && IsConstType(BaseType(srctype)) && !IsConstType(BaseType(desttype))) {
        WARNING(ast, "assignment discards const attribute from pointer");
    }
    if (IsIntType(desttype)) {
        if (IsIntType(srctype)) {
            int lsize = TypeSize(desttype);
            int rsize = TypeSize(srctype);
            if (lsize > rsize) {
                if (IsUnsignedType(srctype)) {
                    ast->right = dopromote(ast->right, rsize, K_ZEROEXTEND);
                } else {
                    ast->right = dopromote(ast->right, rsize, K_SIGNEXTEND);
                }
            }
        }
        if (IsFloatType(srctype)) {
            ERROR(ast, "cannot assign float to integer");
        }
    }
    return desttype;
}

//
// function for doing type checking and various kinds of
// type related manipulations. for example:
//
// signed/unsigned shift: x >> y  => signed shift if x is signed,
//                                   unsigned otherwise
// returns the most recent type signature
//
AST *CheckTypes(AST *ast)
{
    AST *ltype, *rtype;
    if (!ast) return NULL;
    ltype = CheckTypes(ast->left);
    rtype = CheckTypes(ast->right);
    switch (ast->kind) {
    case AST_OPERATOR:
        ltype = CoerceOperatorTypes(ast, ltype, rtype);
        break;
    case AST_ASSIGN:
        ltype = CoerceAssignTypes(ast, ltype, rtype);
        break;
    case AST_FLOAT:
    case AST_TRUNC:
    case AST_ROUND:
        return ast_type_float;
    case AST_ISBETWEEN:
    case AST_INTEGER:
    case AST_HWREG:
    case AST_CONSTREF:
    case AST_CONSTANT:
        return ast_type_long;
    case AST_STRING:
    case AST_STRINGPTR:
        return ast_type_string;
    case AST_ARRAYREF:
        {
            AST *lefttype = ExprType(ast->left);
            AST *basetype;
            if (!lefttype) {
                return NULL;
            }
            basetype = BaseType(lefttype);
            if (IsPointerType(lefttype)) {
                // force this to have a memory dereference
                AST *deref = NewAST(AST_MEMREF, basetype, ast->left);
                ast->left = deref;
            } else if (IsArrayType(lefttype)) {
            } else {
                ERROR(ast, "Array dereferences a non-array object");
                return NULL;
            }
            return basetype;
        }
        break;
    case AST_EXPRLIST:
    case AST_SEQUENCE:
    case AST_FUNCCALL:
    case AST_METHODREF:
    case AST_IDENTIFIER:
        return ExprType(ast);
    default:
        break;
    }
    return ltype;
}

////////////////////////////////////////////////////////////////
static AST *
getBasicPrimitive(const char *name)
{
    AST *ast;

    ast = AstIdentifier(name);
    return ast;
}

void
BasicTransform(Module *Q)
{
    Module *savecur = current;
    Function *func;
    Function *savefunc = curfunc;

    if (gl_fixedreal) {
        basic_print_float = getBasicPrimitive("_basic_print_fixed");
        float_mul = getBasicPrimitive("_fixed_mul");
        float_div = getBasicPrimitive("_fixed_div");
    } else {
        basic_print_float = getBasicPrimitive("_basic_print_float");
    }
    basic_print_integer = getBasicPrimitive("_basic_print_integer");
    basic_print_unsigned = getBasicPrimitive("_basic_print_unsigned");
    basic_print_string = getBasicPrimitive("_basic_print_string");
    basic_print_char = getBasicPrimitive("_basic_print_char");
    basic_print_nl = getBasicPrimitive("_basic_print_nl");

    current = Q;
    for (func = Q->functions; func; func = func->next) {
        curfunc = func;

        doBasicTransform(&func->body);
        CheckTypes(func->body);
    }
    curfunc = savefunc;
    current = savecur;
}
