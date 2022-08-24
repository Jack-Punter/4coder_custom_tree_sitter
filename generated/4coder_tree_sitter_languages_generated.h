extern "C" {
	TSLanguage *tree_sitter_c();
	TSLanguage *tree_sitter_cpp();
	TSLanguage *tree_sitter_odin();
	TSLanguage *tree_sitter_go();
}

static String_Const_u8 language_c_highlight_query = string_u8_litexpr(R"(
"break" @defcolor_keyword
"case" @defcolor_keyword
"const" @defcolor_keyword
"continue" @defcolor_keyword
"default" @defcolor_keyword
"do" @defcolor_keyword
"else" @defcolor_keyword
"enum" @defcolor_keyword
"extern" @defcolor_keyword
"for" @defcolor_keyword
"if" @defcolor_keyword
"inline" @defcolor_keyword
"return" @defcolor_keyword
"sizeof" @defcolor_keyword
"static" @defcolor_keyword
"struct" @defcolor_keyword
"switch" @defcolor_keyword
"typedef" @defcolor_keyword
"union" @defcolor_keyword
"volatile" @defcolor_keyword
"while" @defcolor_keyword

"#define" @defcolor_keyword
"#elif" @defcolor_keyword
"#else" @defcolor_keyword
"#endif" @defcolor_keyword
"#if" @defcolor_keyword
"#ifdef" @defcolor_keyword
"#ifndef" @defcolor_keyword
"#include" @defcolor_keyword
(preproc_directive) @defcolor_keyword

"--" @fleury_color_operators
"-" @fleury_color_operators
"-=" @fleury_color_operators
"->" @fleury_color_operators
"=" @fleury_color_operators
"!=" @fleury_color_operators
"*" @fleury_color_operators
"&" @fleury_color_operators
"&&" @fleury_color_operators
"+" @fleury_color_operators
"++" @fleury_color_operators
"+=" @fleury_color_operators
"<" @fleury_color_operators
"==" @fleury_color_operators
">" @fleury_color_operators
"||" @fleury_color_operators

"." @fleury_color_syntax_crap
";" @fleury_color_syntax_crap

(string_literal) @defcolor_str_constant
(system_lib_string) @defcolor_str_constant

(null) @fleury_color_index_constant
(number_literal) @defcolor_int_constant
(char_literal) @defcolor_int_constant

(field_identifier) @defcolor_text_default
(statement_identifier) @defcolor_text_default
(type_identifier) @fleury_color_index_product_type
(primitive_type) @fleury_color_index_product_type
(sized_type_specifier) @fleury_color_index_product_type

; ((identifier) @fleury_color_index_constant
   ;  (#match? @fleury_color_index_constant "^[A-Z][A-Z\\d_]*$"))
       
       ; (identifier) @defcolor_text_default
       
       (comment) @defcolor_comment
       
       (call_expression
        function: (identifier) @fleury_color_index_function)
       (call_expression
        function: (field_expression
                   field: (field_identifier) @fleury_color_index_function))
       (function_declarator
        declarator: (identifier) @fleury_color_index_function)
       (preproc_function_def
        name: (identifier) @fleury_color_index_function)
       )");

static String_Const_u8 language_cpp_highlight_query = string_u8_litexpr(R"(
"break" @defcolor_keyword
"case" @defcolor_keyword
"const" @defcolor_keyword
"continue" @defcolor_keyword
"default" @defcolor_keyword
"do" @defcolor_keyword
"else" @defcolor_keyword
"enum" @defcolor_keyword
"extern" @defcolor_keyword
"for" @defcolor_keyword
"if" @defcolor_keyword
"inline" @defcolor_keyword
"return" @defcolor_keyword
"sizeof" @defcolor_keyword
"static" @defcolor_keyword
"struct" @defcolor_keyword
"switch" @defcolor_keyword
"typedef" @defcolor_keyword
"union" @defcolor_keyword
"volatile" @defcolor_keyword
"while" @defcolor_keyword

"#define" @defcolor_keyword
"#elif" @defcolor_keyword
"#else" @defcolor_keyword
"#endif" @defcolor_keyword
"#if" @defcolor_keyword
"#ifdef" @defcolor_keyword
"#ifndef" @defcolor_keyword
"#include" @defcolor_keyword
(preproc_directive) @defcolor_keyword

"--" @fleury_color_operators
"-" @fleury_color_operators
"-=" @fleury_color_operators
"->" @fleury_color_operators
"=" @fleury_color_operators
"!=" @fleury_color_operators
"*" @fleury_color_operators
"&" @fleury_color_operators
"&&" @fleury_color_operators
"+" @fleury_color_operators
"++" @fleury_color_operators
"+=" @fleury_color_operators
"<" @fleury_color_operators
"==" @fleury_color_operators
">" @fleury_color_operators
"||" @fleury_color_operators

"." @fleury_color_syntax_crap
";" @fleury_color_syntax_crap

(string_literal) @defcolor_str_constant
(system_lib_string) @defcolor_str_constant

(null) @fleury_color_index_constant
(number_literal) @defcolor_int_constant
(char_literal) @defcolor_int_constant

(field_identifier) @defcolor_text_default
(statement_identifier) @defcolor_text_default
(type_identifier) @fleury_color_index_product_type
(primitive_type) @fleury_color_index_product_type
(sized_type_specifier) @fleury_color_index_product_type

; ((identifier) @fleury_color_index_constant
   ;  (#match? @fleury_color_index_constant "^[A-Z][A-Z\\d_]*$"))
       
       ; (identifier) @defcolor_text_default
       
       (comment) @defcolor_comment
       
       (call_expression
        function: (identifier) @fleury_color_index_function)
       (call_expression
        function: (field_expression
                   field: (field_identifier) @fleury_color_index_function))
       (function_declarator
        declarator: (identifier) @fleury_color_index_function)
       (preproc_function_def
        name: (identifier) @fleury_color_index_function)
       

; Types

((namespace_identifier) @fleury_color_index_product_type
 (#match? @fleury_color_index_product_type "^[A-Z]"))
  
  (auto) @fleury_color_index_product_type
  
  ; Constants
  
  (this) @defcolor_text_default
  (nullptr) @defcolor_str_constant
  
  ; Keywords
  
  [
   "catch"
   "class"
   "co_await"
   "co_return"
   "co_yield"
   "constexpr"
   "constinit"
   "consteval"
   "delete"
   "explicit"
   "final"
   "friend"
   "mutable"
   "namespace"
   "noexcept"
   "new"
   "override"
   "private"
   "protected"
   "public"
   "template"
   "throw"
   "try"
   "typename"
   "using"
   "virtual"
   "concept"
   "requires"
   ] @defcolor_keyword
  
  ; Strings
  
  (raw_string_literal) @defcolor_string_constant
  
  ; Functions
  
  (call_expression
   function: (qualified_identifier
              name: (identifier) @fleury_color_index_function))
  
  (template_function
   name: (identifier) @fleury_color_index_function)
  
  (template_method
   name: (field_identifier) @fleury_color_index_function)
  
  (template_function
   name: (identifier) @fleury_color_index_function)
  
  (function_declarator
   declarator: (qualified_identifier
                name: (identifier) @fleury_color_index_function))
  
  (function_declarator
   declarator: (qualified_identifier
                name: (identifier) @fleury_color_index_function))
  
  (function_declarator
   declarator: (field_identifier) @fleury_color_index_function)
  
  )");

static String_Const_u8 language_odin_highlight_query = string_u8_litexpr(R"(
(keyword) @defcolor_keyword
(operator) @fleury_color_operators

(int_literal)   @defcolor_int_constant
(float_literal) @defcolor_int_constant
(rune_literal)  @defcolor_int_constant
(bool_literal) @defcolor_bool_constant
(nil) @constant.builtin

(ERROR) @fleury_color_error_annotation

(type_identifier)    @fleury_color_index_product_type
(package_identifier) @fleury_color_index_constant
(label_identifier)   @label

(interpreted_string_literal) @defcolor_str_constant
(raw_string_literal) @defcolor_str_constant
(escape_sequence) @fleury_color_lego_grab

(comment) @defcolor_comment
(const_identifier) @fleury_color_index_constant

(compiler_directive) @fleury_color_index_macro
(calling_convention) @fleury_color_index_macro

(identifier) @defcolor_text_default
(pragma_identifier) @fleury_color_index_macro
)");

static String_Const_u8 language_go_highlight_query = string_u8_litexpr(R"(

; Identifiers

(type_identifier) @fleury_color_index_product_type
(field_identifier) @defcolor_text_default
(identifier) @defcolor_text_default

; Operators

[
 "--"
 "-"
 "-="
 ":="
 "!"
 "!="
 "..."
 "*"
 "*"
 "*="
 "/"
 "/="
 "&"
 "&&"
 "&="
 "%"
 "%="
 "^"
 "^="
 "+"
 "++"
 "+="
 "<-"
 "<"
 "<<"
 "<<="
 "<="
 "="
 "=="
 ">"
 ">="
 ">>"
 ">>="
 "|"
 "|="
 "||"
 "~"
 ] @fleury_color_operators

; Keywords

[
 "break"
 "case"
 "chan"
 "const"
 "continue"
 "default"
 "defer"
 "else"
 "fallthrough"
 "for"
 "func"
 "go"
 "goto"
 "if"
 "import"
 "interface"
 "map"
 "package"
 "range"
 "return"
 "select"
 "struct"
 "switch"
 "type"
 "var"
 ] @defcolor_keyword

; Literals

[
 (interpreted_string_literal)
 (raw_string_literal)
 (rune_literal)
 ] @defcolor_str_constant

(escape_sequence) @fleury_color_index_constant

[
 (int_literal)
 (float_literal)
 (imaginary_literal)
 ] @defcolor_int_constant

[
 (true)
 (false)
 (nil)
 (iota)
 ] @defcolor_str_constant

(comment) @defcolor_comment

; Function calls

(call_expression
 function: (identifier) @fleury_color_index_function)

(call_expression
 function: (selector_expression
            field: (field_identifier) @fleury_color_index_function))

; Function definitions

(function_declaration
 name: (identifier) @fleury_color_index_function)

(method_declaration
 name: (field_identifier) @fleury_color_index_function)

; Custom
(qualified_type
 package: (package_identifier) @fleury_color_index_product_type)

)");

function void
jpts_register_languages(Application_Links *app) {
	Scratch_Block scratch(app);

	{
		List_String_Const_u8 extension_list = {};
		string_list_push(scratch, &extension_list, string_u8_litexpr("c"));
		jpts_register_language_group(app, extension_list, language_c_highlight_query, tree_sitter_c());
	}

	{
		List_String_Const_u8 extension_list = {};
		string_list_push(scratch, &extension_list, string_u8_litexpr("cpp"));
		string_list_push(scratch, &extension_list, string_u8_litexpr("cc"));
		string_list_push(scratch, &extension_list, string_u8_litexpr("hpp"));
		string_list_push(scratch, &extension_list, string_u8_litexpr("cxx"));
		string_list_push(scratch, &extension_list, string_u8_litexpr("h"));
		jpts_register_language_group(app, extension_list, language_cpp_highlight_query, tree_sitter_cpp());
	}

	{
		List_String_Const_u8 extension_list = {};
		string_list_push(scratch, &extension_list, string_u8_litexpr("odin"));
		jpts_register_language_group(app, extension_list, language_odin_highlight_query, tree_sitter_odin());
	}

	{
		List_String_Const_u8 extension_list = {};
		string_list_push(scratch, &extension_list, string_u8_litexpr("go"));
		jpts_register_language_group(app, extension_list, language_go_highlight_query, tree_sitter_go());
	}
}
