#ifndef CUSTOM_STYLES_H
#define CUSTOM_STYLES_H

#include <Arduino.h>

const char CUSTOM_HEAD_ELEMENT[] PROGMEM = R"rawliteral(
<style>
.btn,.btn a,.msg{color:#fff}body,input,select,textarea{background:#121212;color:#fff;border:none;border-radius:6px;font:16px 'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;margin:0}input,select,textarea{outline:0;font-size:14px;border:1px solid #333;padding:10px;width:90%;background:#1e1e1e}.btn a{text-decoration:none}.container{margin:auto;width:90%;max-width:600px;display:flex;justify-content:center;align-items:center}.btn{background:linear-gradient(45deg,#e86522,#e89922);cursor:pointer;display:inline-block;margin:10px 0;padding:12px 20px;width:100%;border-radius:8px;box-shadow:0 4px 6px rgba(0,0,0,.3);transition:transform .2s,box-shadow .2s;font-weight:700;text-align:center}.btn:hover{background:linear-gradient(45deg,#e89922,#e86522);transform:translateY(-2px);box-shadow:0 6px 8px rgba(0,0,0,.4)}.btn:active,.btn:focus{background:#da5c1e;transform:translateY(0);box-shadow:0 4px 6px rgba(0,0,0,.3)}input:focus,select:focus,textarea:focus{border-color:#e86522}.msg{background:#333;border-left:5px solid #e86522;padding:1.5em;border-radius:4px;margin-top:10px}input[type=checkbox]{float:left;width:20px;accent-color:#E86522;transform:scale(1.2)}.table tbody>:nth-child(2n-1){background:#1e1e1e}a{color:#e86522}fieldset{border:1px solid #333;border-radius:8px;margin:20px 0;padding:1em;box-shadow:0 2px 4px rgba(0,0,0,.5)}
</style>
)rawliteral";

#endif // CUSTOM_STYLES_H