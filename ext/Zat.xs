#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "../src/zat.h"

MODULE = Zat		PACKAGE = Zat		

SV *
zat (...)
  PPCODE:
	unsigned process_ins(SV ***ins_ptr, Bytef **buf_ptr) {
		SV *sv = **ins_ptr;
		if (sv == NULL) return 0;
		STRLEN len;
		*buf_ptr = SvPV(sv,len);
		(*ins_ptr)++;
		return len;
	}

	int level = Z_DEFAULT_COMPRESSION;
    uint32_t i = 0,j;
	if (items && SvIOKp(ST(0))) {
		level = SvIV(ST(0));
		++i;
	}
	SV **ins = (SV **)alloca((items - i + 1) * sizeof (SV *));
    for (j = 0; i < items; i++) {
		if (!SvOK(ST(i))) continue;
		ins[j++] = ST(i);
    }
	ins[j] = NULL;

	zatbuf zbuf = { };

	int status = zat_exec( level, NULL, 0, (in_func) process_ins, & ins, (out_func) zatbuf_out, & zbuf);
	PUSHs(sv_2mortal( newSViv( status != Z_STREAM_END ) ));

	SV *zdata = newSV(0);
	sv_usepvn(zdata, zbuf.data, zbuf.size);
	PUSHs(sv_2mortal(zdata));
