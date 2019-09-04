
#include "../libjpeg-turbo/jcapimin.c"
#include "../libjpeg-turbo/jcapistd.c"

#define my_coef_controller my_coef_controller_jccoefct
#define my_coef_ptr my_coef_ptr_jccoefct
#define start_iMCU_row start_iMCU_row_jccoefct
#define start_pass_coef start_pass_coef_jccoefct
#define compress_output compress_output_jccoefct
#define my_master_ptr my_master_ptr_jccoefct
#define my_main_controller my_main_controller_jccoefct
#include "../libjpeg-turbo/jccoefct.c"
#undef my_coef_controller
#undef my_coef_ptr
#undef start_iMCU_row
#undef start_pass_coef
#undef compress_output
#undef my_master_ptr
#undef my_main_controller

#define jpeg_nbits_table jpeg_nbits_table_jccolor
#define c_derived_tbl c_derived_tbl_jccolor
#define my_cconvert_ptr my_cconvert_ptr_jccolor
#include "../libjpeg-turbo/jccolor.c"
#undef c_derived_tbl
#undef jpeg_nbits_table
#undef FIX
#undef my_cconvert_ptr
#undef TABLE_SIZE
#include "../libjpeg-turbo/jcdctmgr.c"
#undef FIX
#define jpeg_nbits_table jpeg_nbits_table_jchuff
#define c_derived_tbl c_derived_tbl_jchuff
#include "../libjpeg-turbo/jchuff.c"
#undef c_derived_tbl
#undef jpeg_nbits_table
#undef emit_byte
#undef BUFSIZE
#undef NEG_1
#undef HUFF_EXTEND
#include "../libjpeg-turbo/jcicc.c"
#include "../libjpeg-turbo/jcinit.c"

#define my_master_ptr my_master_ptr_jcmainct
#define my_main_controller my_main_controller_jcmainct
#include "../libjpeg-turbo/jcmainct.c"
#undef my_main_controller
#undef my_master_ptr

#include "../libjpeg-turbo/jcmarker.c"

#define my_master_ptr my_master_ptr_jcmaster
#include "../libjpeg-turbo/jcmaster.c"
#undef my_master_ptr

#include "../libjpeg-turbo/jcomapi.c"
#include "../libjpeg-turbo/jcparam.c"
#define c_derived_tbl c_derived_tbl_jcphuff
#define jpeg_nbits_table jpeg_nbits_table_jcphuff
#include "../libjpeg-turbo/jcphuff.c"
#undef c_derived_tbl
#undef jpeg_nbits_table
#undef emit_byte
#undef BUFSIZE
#undef NEG_1
#undef HUFF_EXTEND
#include "../libjpeg-turbo/jcprepct.c"
#define jpeg_nbits_table jpeg_nbits_table_jcsample
#define c_derived_tbl c_derived_tbl_jcsample
#include "../libjpeg-turbo/jcsample.c"
#undef c_derived_tbl
#undef jpeg_nbits_table

#define my_coef_controller my_coef_controller_jctrans
#define my_coef_ptr my_coef_ptr_jctrans
#define start_iMCU_row start_iMCU_row_jctrans
#define start_pass_coef start_pass_coef_jctrans
#define compress_output compress_output_jctrans
#define my_master_ptr my_master_ptr_jctrans
#define my_main_controller my_main_controller_jctrans
#include "../libjpeg-turbo/jctrans.c"
#undef my_coef_controller
#undef my_coef_ptr
#undef start_iMCU_row
#undef start_pass_coef
#undef compress_output
#undef my_master_ptr
#undef my_main_controller

#define my_master_ptr my_master_ptr_jdapimin
#include "../libjpeg-turbo/jdapimin.c"
#undef my_master_ptr

#define my_main_ptr my_main_ptr_jdapistd
#define my_main_controller my_main_controller_jdapistd
#define my_coef_controller my_coef_controller_jdapistd
#define my_coef_ptr my_coef_ptr_jdapistd
#define start_iMCU_row start_iMCU_row_jdapistd
#include "../libjpeg-turbo/jdapistd.c"
#undef my_main_controller
#undef my_main_ptr
#undef my_coef_controller
#undef my_coef_ptr
#undef start_iMCU_row



#include "../libjpeg-turbo/jdatadst.c"
#include "../libjpeg-turbo/jdatasrc.c"

#define my_coef_ptr my_coef_ptr_jdcoefct
#define my_coef_controller my_coef_controller_jdcoefct
#define start_iMCU_row start_iMCU_row_jdcoefct
#include "../libjpeg-turbo/jdcoefct.c"
#undef my_coef_controller
#undef my_coef_ptr
#undef start_iMCU_row
#undef JCONFIG_INCLUDED

#define my_cconvert_ptr my_cconvert_ptr_jdcolor
#define RGB_RED         0
#define RGB_GREEN       1
#define RGB_BLUE        2
#define RGB_PIXELSIZE   3
#include "../libjpeg-turbo/jdcolor.c"
#undef FIX
#undef TABLE_SIZE
#undef my_cconvert_ptr

#include "../libjpeg-turbo/jddctmgr.c"
#undef FIX
#undef CONST_BITS
#undef DESCALE
#include "../libjpeg-turbo/jdhuff.c"
#undef emit_byte
#undef BUFSIZE
#undef NEG_1
#undef HUFF_EXTEND

#include "../libjpeg-turbo/jdicc.c"
#include "../libjpeg-turbo/jdinput.c"
#include "../libjpeg-turbo/jdmainct.c"
#include "../libjpeg-turbo/jdmarker.c"
#include "../libjpeg-turbo/jdmaster.c"
#include "../libjpeg-turbo/jdmerge.c"
#undef FIX
#include "../libjpeg-turbo/jdphuff.c"
#undef emit_byte
#undef BUFSIZE
#undef NEG_1
#undef HUFF_EXTEND
#include "../libjpeg-turbo/jdpostct.c"
#include "../libjpeg-turbo/jdsample.c"
#include "../libjpeg-turbo/jdtrans.c"
#include "../libjpeg-turbo/jerror.c"
#include "../libjpeg-turbo/jfdctflt.c"
#include "../libjpeg-turbo/jfdctfst.c"
#undef CONST_BITS
#undef DESCALE
#undef FIX_0_541196100
#undef FIX_1_847759065
#undef MULTIPLY
#undef DEQUANTIZE

#include "../libjpeg-turbo/jfdctint.c"
#undef CONST_BITS
#undef DESCALE
#undef FIX_0_541196100
#undef FIX_1_847759065
#undef MULTIPLY
#undef DEQUANTIZE

#include "../libjpeg-turbo/jidctflt.c"
#undef DEQUANTIZE

#include "../libjpeg-turbo/jidctfst.c"
#undef CONST_BITS
#undef DESCALE
#undef FIX_0_541196100
#undef FIX_1_847759065
#undef MULTIPLY
#undef DEQUANTIZE


#include "../libjpeg-turbo/jidctint.c"
#undef CONST_BITS
#undef DESCALE
#undef FIX_0_541196100
#undef FIX_1_847759065
#undef MULTIPLY
#undef DEQUANTIZE

#include "../libjpeg-turbo/jidctred.c"
#include "../libjpeg-turbo/jquant1.c"
#include "../libjpeg-turbo/jquant2.c"
#include "../libjpeg-turbo/jutils.c"
#include "../libjpeg-turbo/jmemmgr.c"
#include "../libjpeg-turbo/jmemnobs.c"
#include "../libjpeg-turbo/jaricom.c"
#include "../libjpeg-turbo/jcarith.c"
#include "../libjpeg-turbo/jdarith.c"
