#pragma once

typedef enum CToolchainNodeType
{
    C_CMD_COMPILE = 1,
    C_CMD_LINK,
    C_CMD_AR,
    C_CMD_MAKE_IMPLIB,
    C_CMD_BMI_TO_OBJ,
    C_CMD_SCAN_DEPS,
    C_CMD_SCAN_TESTS,
    C_VIRTUAL_MAKE_COMPILE_CMDLINE,
    C_FILE_TEST,
} CToolchainNodeType;
