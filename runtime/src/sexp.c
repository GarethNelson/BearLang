#include <bearlang/common.h>
#include <bearlang/sexp.h>
#include <bearlang/list_ops.h>
#include <bearlang/utils.h>
#include <bearlang/error_tools.h>
#include <bl_lexer.h>
#include <stdio.h>
#include <gc.h>


bl_val_t* read_form(yyscan_t scanner);

bl_val_t* read_list(yyscan_t scanner) { // LCOV_EXCL_LINE

    bl_val_t* retval = bl_mk_val(BL_VAL_TYPE_CONS);
    retval->car = read_form(scanner);
    retval->cdr = NULL;
//    if(retval == NULL) return retval; 
    if(retval->car == NULL) return retval;
    if(retval->car->type == BL_VAL_TYPE_LIST_END) {
       retval->car = NULL;
       return retval;
    }
    


    bl_val_t* L = retval;
    bl_val_t* e = bl_mk_null();
    while(e->type != BL_VAL_TYPE_LIST_END) {
       e = read_form(scanner);
       if(e==NULL) return retval;
       if(e->type != BL_VAL_TYPE_LIST_END) {
          L->cdr = bl_mk_val(BL_VAL_TYPE_CONS);
	  L->cdr->car = e;
	  L->cdr->cdr = NULL;
	  L = L->cdr;
       }
    }
    return retval;
}

char* unescape(char* s) { // LCOV_EXCL_LINE

    size_t count = strlen(s);
    char* retval = GC_MALLOC_ATOMIC(sizeof(s) * count+1);
    bool slash;
    int i, j;
    for(i=j=0; s[i] != '\0'; i++) {
        switch(s[i]) {
	   case '\\':
	     slash = true;
	   break;
	   case 'n':
	      if(slash) {
	         retval[j++] = '\n';
		 slash = false;
	         break;
	      }
	   case 't':
	      if(slash) {
		 retval[j++] = '\t';
		 slash = false;
	         break;
	      }
	   default:
	      retval[j++] = s[i];
	      break;
	}
    }
    retval[j] = '\0';
    return retval;
}

bl_val_t if_oper_val  = {.type = BL_VAL_TYPE_OPER_IF};
bl_val_t do_oper_val  = {.type = BL_VAL_TYPE_OPER_DO};
bl_val_t end_list_val = {.type = BL_VAL_TYPE_LIST_END};

bl_val_t while_oper_val = {.type = BL_VAL_TYPE_OPER_WHILE};

bl_val_t* read_form(yyscan_t scanner) { // LCOV_EXCL_LINE

    char* s = NULL;
    bl_token_type_t tok = yylex(scanner);
    if(tok == 0) return NULL;
    bl_val_t* retval = NULL;
    switch(tok) {
	case BL_TOKEN_STRING:
		retval = bl_mk_str(unescape(yyget_text(scanner)));
	break;
	case BL_TOKEN_LPAREN:
		retval = read_list(scanner);
	break;
	case BL_TOKEN_RPAREN:
		retval = &end_list_val;
	break;
	case BL_TOKEN_INTEGER:
		retval = bl_mk_integer(yyget_text(scanner));
	break;
	case BL_TOKEN_SYMBOL:
		s = yyget_text(scanner);
		retval = bl_mk_symbol(yyget_text(scanner));
	break;
	case BL_TOKEN_IF:
		retval = &if_oper_val;
	break;
	case BL_TOKEN_DO:
		retval = &do_oper_val;
	break;
	case BL_TOKEN_WHILE:
		retval = &while_oper_val;
	break;
	case BL_TOKEN_DOCSTRING:
		retval = bl_mk_docstr(yyget_text(scanner));
	break;
   }
   retval->line_num = yyget_lineno(scanner);
   return retval;
}

bl_val_t* bl_parse_sexp(char* sexp) { // LCOV_EXCL_LINE

   yyscan_t scanner;
   yylex_init(&scanner);
   yy_scan_string(sexp,scanner);
   bl_val_t* retval = read_form(scanner);
   yylex_destroy(scanner);
   if(retval==NULL) return bl_mk_null();
   return retval;
}

bl_val_t* bl_parse_file(char* filename, FILE* fd) { // LCOV_EXCL_LINE

   yyscan_t scanner;
   yylex_init(&scanner);
   yyset_in(fd,scanner);
   bl_val_t* retval = read_list(scanner);
   yylex_destroy(scanner);
   return retval;
}

// LCOV_EXCL_START
char* bl_ser_sexp(bl_val_t* expr) {

      if(expr == NULL) return "";
      char* retval=GC_MALLOC_ATOMIC(4096);
      char* s="";
      switch(expr->type) {
         case BL_VAL_TYPE_ANY:
	   snprintf(retval,5,"<any>");
	 break;
         case BL_VAL_TYPE_OPER_IF:
	   snprintf(retval,3,"if");
	 break;
	 case BL_VAL_TYPE_FUNC_NATIVE:
	   snprintf(retval,32,"<nativefunction>");
	 case BL_VAL_TYPE_NULL:
           snprintf(retval, 5, "None");
         break;
	 case BL_VAL_TYPE_CTX:
	   snprintf(retval,6,"<ctx>");
	 break;
	 case BL_VAL_TYPE_ERROR:
           retval = bl_errmsg(expr);
	 break;
         case BL_VAL_TYPE_SYMBOL:
           snprintf(retval,strlen(expr->s_val)+1,"%s",expr->s_val);
         break;
         case BL_VAL_TYPE_STRING:
           snprintf(retval,strlen(expr->s_val)+3,"\"%s\"",expr->s_val);
         break;
         case BL_VAL_TYPE_NUMBER:
	   snprintf(retval,10,"%lld", expr->fix_int);
	 //            retval = mpz_get_str(NULL, 10, expr->i_val);
         break;
	 case BL_VAL_TYPE_BOOL:
           if(expr->b_val) {
              snprintf(retval,10,"True");
	   } else {
              snprintf(retval,10,"False");
	   }
	 break;
         case BL_VAL_TYPE_FUNC_BL:
	   snprintf(retval,1024,"(fn %s %s)",bl_ser_sexp(expr->bl_funcargs_ptr), bl_ser_sexp(expr->bl_func_ptr));
	 break;
         case BL_VAL_TYPE_OPER_NATIVE:
	   snprintf(retval,32,"<nativeoper>");
	 break;
	 case BL_VAL_TYPE_CONS:
	   snprintf(retval,2,"%s","(");
           if(expr->car == NULL)  {
     	       snprintf(retval,4,"%s","()");
	   } else {
               bl_val_t* L=expr;
	       for(L=expr; L!=NULL; L=L->cdr) {
                     char* newval = bl_ser_sexp(L->car);
		     retval = GC_REALLOC(retval,strlen(retval)+strlen(newval)+4);
		     retval = strcat(retval,newval);
		     retval = strcat(retval," ");
	       }
	       retval[strlen(retval)-1]=')';
	   }

         break;
      }
      return retval;
}

// LCOV_EXCL_STOP

char* bl_ser_naked_sexp(bl_val_t* expr) { // LCOV_EXCL_LINE

      char* retval = bl_ser_sexp(expr);
      if(expr->type == BL_VAL_TYPE_STRING) {
         retval++;
	 retval[strlen(retval)-1]='\0';
      }
      return retval++;
}

