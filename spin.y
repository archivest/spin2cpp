/*
 * Spin compiler parser
 * Copyright (c) 2011 Total Spectrum Software Inc.
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include "spinc.h"

/* Yacc functions */
    void yyerror(char *);
    int yylex(void);
%}

%token T_IDENTIFIER
%token T_NUM

/* various keywords */
%token T_CON
%token T_VAR
%token T_DAT
%token T_PUB
%token T_PRI
%token T_OBJ

%token T_BYTE
%token T_WORD
%token T_LONG

%token T_REPEAT
%token T_IF
%token T_IFNOT
%token T_ELSE
%token T_ELSEIF
%token T_ELSEIFNOT

/* other stuff */
%token T_RETURN
%token T_INDENT
%token T_OUTDENT
%token T_EOLN
%token T_EOF

/* operators */
%right T_ASSIGN
%left '<' '>' T_GE T_LE T_NE T_EQ
%left '-' '+'
%left '*' '/' T_MODULUS T_HIGHMULT

%%
input:
  topelement
  | topelement input
  ;

topelement:
  T_EOLN
  | T_CON T_EOLN conblock
  { $$ = current->conblock = AddToList(current->conblock, $3); }
  | T_DAT T_EOLN datblock
  { $$ = current->datblock = AddToList(current->datblock, $3); }
  | T_VAR T_EOLN varblock
  { $$ = current->varblock = AddToList(current->varblock, $3); }
  | T_PUB funcdef stmtlist
  { DeclareFunction(1, $2, $3); }
  | T_PRI funcdef stmtlist
  { DeclareFunction(0, $2, $3); }
;

funcdef:
  T_IDENTIFIER optparamlist T_EOLN
  { AST *funcdecl = NewAST(AST_FUNCDECL, $1, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $2, NULL);
    $$ = NewAST(AST_FUNCDEF, funcdecl, funcvars);
  }
|  T_IDENTIFIER optparamlist localvars T_EOLN
  { AST *funcdecl = NewAST(AST_FUNCDECL, $1, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $2, $3);
    $$ = NewAST(AST_FUNCDEF, funcdecl, funcvars);
  }
|  T_IDENTIFIER optparamlist resultname localvars T_EOLN
  { AST *funcdecl = NewAST(AST_FUNCDECL, $1, $3);
    AST *funcvars = NewAST(AST_FUNCVARS, $2, $4);
    $$ = NewAST(AST_FUNCDEF, funcdecl, funcvars);
  }

;

optparamlist:
/* nothing */
  { $$ = NULL; }
| identlist
  { $$ = $1; }
  ;

resultname: ':' T_IDENTIFIER
  { $$ = $2; }
  ;

localvars:
 '|' identlist
  { $$ = $2 }
    ;

stmtlist:
  stmt
  | stmtlist stmt
  ;

stmt:
   T_RETURN T_EOLN
|  T_RETURN expr T_EOLN
|  ifstmt
|  stmtblock
  ;

stmtblock:
  T_INDENT stmtlist T_OUTDENT
  { $$ = $2; }
;

ifstmt:
  iforifnot expr T_EOLN stmtblock
  ;

iforifnot:
  T_IF
| T_IFNOT
  ;

 
conblock:
  conline
  { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | conblock conline
  { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $2, NULL)); }
  ;

conline:
  identifier '=' expr T_EOLN
     { $$ = NewAST(AST_ASSIGN, $1, $3); }
  | enumlist T_EOLN
     { $$ = $1; }
  ;

enumlist:
  enumitem
  | enumlist ',' enumitem
    { $$ = AddToList($1, $3); }
  ;

enumitem:
  identifier
  { $$ = $1; }
  | '#' expr
  { $$ = NewAST(AST_ENUMSET, $2, NULL); }
  ;

datblock:
  datline
  | datblock datline
  ;

datline:
  optsymbol sizespec datalist T_EOLN
  ;

datalist:
  dataelem
  | datalist ',' dataelem
  ;

dataelem:
  optsize expr optcount
;

varblock:
    varline
    { $$ = $1; }
  | varblock varline
    { $$ = AddToList($1, $2); }
  ;

varline:
  T_BYTE identlist T_EOLN
    { $$ = NewAST(AST_BYTELIST, $2, NULL); }
  | T_WORD identlist T_EOLN
    { $$ = NewAST(AST_WORDLIST, $2, NULL); }
  | T_LONG identlist T_EOLN
    { $$ = NewAST(AST_LONGLIST, $2, NULL); }
  ;

identlist:
  identdecl
  { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | identlist ',' identdecl
  { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
  ;

identdecl:
  identifier
  { $$ = $1; }
  | identifier '[' expr ']'
  { $$ = NewAST(AST_ARRAYDECL, $1, $3); }
  ;

optsize:
  | sizespec
  ;

optcount:
  | '[' expr ']'
  ;

optsymbol:
  | identifier
  ;

expr:
  integer
  | lhs
  | lhs T_ASSIGN expr
    { $$ = NewAST(AST_ASSIGN, $1, $3); $$->d.ival = T_ASSIGN; }
  | expr '+' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '+'; }
  | expr '-' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '-'; }
  | expr '*' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '*'; }
  | expr '/' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '/'; }
  | expr '>' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '>'; }
  | expr '<' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '<'; }
  | expr T_GE expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_GE; }
  | expr T_LE expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_LE; }
  | expr T_NE expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_NE; }
  | expr T_EQ expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_EQ; }
  | expr T_MODULUS expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_MODULUS; }
  | expr T_HIGHMULT expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_HIGHMULT; }
  | '(' expr ')'
  ;

lhs: identifier
  | lhs '[' expr ']'
    { $$ = NewAST(AST_ARRAYDECL, $1, $3); }
  ;

sizespec:
  T_BYTE
  | T_WORD
  | T_LONG
  ;

integer:
  T_NUM
  { $$ = current->ast; }
;

identifier:
  T_IDENTIFIER
  { $$ = current->ast; }
;
 
%%

void
yyerror(char *msg)
{
    fprintf(stderr, "error %s\n", msg);
}
