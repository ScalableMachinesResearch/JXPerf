#ifndef _INSTRUCTION_CODEC_H
#define _INSTRUCTION_CODEC_H

extern "C" {
#include "xed/xed-interface.h"
}
class InstructionCodec {
public:
    static void init();
    static void shutdown();

    static xed_error_enum_t xed_decode_wrapper( xed_decoded_inst_t *xedd, const xed_uint8_t* itext, const unsigned int  bytes);
private:
    static xed_state_t xed_machine_state;
};

#endif /*_INSTRUCTION_DECODE_H*/
