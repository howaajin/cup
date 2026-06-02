#include "cup.h"

Node* provider_bmi = NULL;
Node* provider_bmi_cpp26 = NULL;

ENTRY(build_provider_cpp23, PRIORITY_BEFORE_DEFAULT)
{
    Node* src = SRC("provider.cppm");
    Node* obj = OBJ(src);
    Node* cc = CC(src, obj);
    provider_bmi = module_from_src(src);
    c_compile_cmd_set_export(cc, "provider", provider_bmi);
    c_compile_cmd_set_cpp_std(cc, CPP_LANGUAGE_STANDARD_23);
    Node* lib = LIB("{out_dir}/provider");
    Node* ar = AR(lib);
    ar_cmd_add_input(ar, obj);
}

ENTRY(build_provider_cpp26, PRIORITY_BEFORE_DEFAULT)
{
    Node* src = SRC("provider.cppm");
    Node* obj = obj_from_src_with_variant(src, "_cpp26");
    provider_bmi_cpp26 = module_from_src_with_variant(src, "_cpp26");
    Node* cc = CC(src, obj);
    c_compile_cmd_set_export(cc, "provider", provider_bmi_cpp26);
    c_compile_cmd_set_cpp_std(cc, CPP_LANGUAGE_STANDARD_26);
}

static void build_consumer(char const* name, bool use_cpp26)
{
    Node* src = SRC("{}.cpp", name);
    Node* obj = OBJ(src);
    Node* cc = CC(src, obj);
    c_compile_cmd_set_cpp_std(cc, use_cpp26 ? CPP_LANGUAGE_STANDARD_26 : CPP_LANGUAGE_STANDARD_23);
    c_compile_cmd_add_import(cc, "provider", use_cpp26 ? provider_bmi_cpp26 : provider_bmi);
    Node* lib = LIB("{out_dir}/{}", name);
    Node* ar = AR(lib);
    ar_cmd_add_input(ar, obj);
}

ENTRY(build_consumer_one)
{
    build_consumer("consumer_one", false);
}

ENTRY(build_consumer_two)
{
    build_consumer("consumer_two", true);
}

ENTRY(build_consumer_three)
{
    build_consumer("consumer_three", true);
}

int main(int argc, char** argv)
{
    return execute();
}
