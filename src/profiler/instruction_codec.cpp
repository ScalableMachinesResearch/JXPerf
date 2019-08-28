#include "instruction_codec.h"
#include "config.h"

xed_state_t InstructionCodec::xed_machine_state;

void InstructionCodec::init(){
    xed_tables_init();
    xed_machine_state = 
#if defined (HOST_CPU_x86_64)
 { XED_MACHINE_MODE_LONG_64,  
   XED_ADDRESS_WIDTH_64b };
#else
 { XED_MACHINE_MODE_LONG_COMPAT_32,
   XED_ADDRESS_WIDTH_32b };
#endif
}

void InstructionCodec::shutdown() {
}

xed_error_enum_t InstructionCodec::xed_decode_wrapper( xed_decoded_inst_t *xedd, const xed_uint8_t* itext, const unsigned int  bytes) {
    xed_decoded_inst_zero_set_mode(xedd,&xed_machine_state);
    return xed_decode(xedd, itext, bytes);
}
