.intel_syntax noprefix

.extern MessageBoxA
.extern ExitProcess

.section .rdata
title:
    .asciz "Demo"

text:
    .asciz "Hello, Windows!"

.section .text
.global WinMainCRTStartup

WinMainCRTStartup:
    sub rsp, 40

    xor rcx, rcx
    lea rdx, [rip + text]
    lea r8,  [rip + title]
    xor r9d, r9d

    call MessageBoxA

    xor ecx, ecx
    call ExitProcess
