option casemap:none

extern MessageBoxA : proc
extern ExitProcess : proc

.const
msg_title   byte "Demo", 0
msg_text    byte "Hello, Windows!", 0

.code
hello proc
    sub     rsp, 28h
    xor     rcx, rcx
    lea     rdx, msg_text
    lea     r8,  msg_title
    xor     r9d, r9d
    call    MessageBoxA
    xor     ecx, ecx
    call    ExitProcess
hello endp
end
