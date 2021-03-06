#include <string.h>

#include "nlr.h"
#include "misc.h"
#include "mpconfig.h"
#include "mpqstr.h"
#include "obj.h"
#include "stream.h"

// This file defines generic Python stream read/write methods which
// dispatch to the underlying stream interface of an object.

static mp_obj_t stream_read(mp_obj_t self_in, mp_obj_t arg) {
    struct _mp_obj_base_t *o = (struct _mp_obj_base_t *)self_in;
    if (o->type->stream_p.read == NULL) {
        // CPython: io.UnsupportedOperation, OSError subclass
        nlr_jump(mp_obj_new_exception_msg(MP_QSTR_OSError, "Operation not supported"));
    }

    machine_int_t sz = mp_obj_get_int(arg);
    // +1 because so far we mark end of string with \0
    char *buf = m_new(char, sz + 1);
    int error;
    machine_int_t out_sz = o->type->stream_p.read(self_in, buf, sz, &error);
    if (out_sz == -1) {
        nlr_jump(mp_obj_new_exception_msg_varg(MP_QSTR_OSError, "[Errno %d]", error));
    } else {
        buf[out_sz] = 0;
        return mp_obj_new_str(qstr_from_str_take(buf, /*out_sz,*/ sz + 1));
    }
}

static mp_obj_t stream_write(mp_obj_t self_in, mp_obj_t arg) {
    struct _mp_obj_base_t *o = (struct _mp_obj_base_t *)self_in;
    if (o->type->stream_p.write == NULL) {
        // CPython: io.UnsupportedOperation, OSError subclass
        nlr_jump(mp_obj_new_exception_msg(MP_QSTR_OSError, "Operation not supported"));
    }

    const char *buf = qstr_str(mp_obj_get_qstr(arg));
    machine_int_t sz = strlen(buf);
    int error;
    machine_int_t out_sz = o->type->stream_p.write(self_in, buf, sz, &error);
    if (out_sz == -1) {
        nlr_jump(mp_obj_new_exception_msg_varg(MP_QSTR_OSError, "[Errno %d]", error));
    } else {
        // http://docs.python.org/3/library/io.html#io.RawIOBase.write
        // "None is returned if the raw stream is set not to block and no single byte could be readily written to it."
        // Do they mean that instead of 0 they return None?
        return MP_OBJ_NEW_SMALL_INT(out_sz);
    }
}

// TODO: should be in mpconfig.h
#define READ_SIZE 256
static mp_obj_t stream_readall(mp_obj_t self_in) {
    struct _mp_obj_base_t *o = (struct _mp_obj_base_t *)self_in;
    if (o->type->stream_p.read == NULL) {
        // CPython: io.UnsupportedOperation, OSError subclass
        nlr_jump(mp_obj_new_exception_msg(MP_QSTR_OSError, "Operation not supported"));
    }

    int total_size = 0;
    vstr_t *vstr = vstr_new_size(READ_SIZE);
    char *buf = vstr_str(vstr);
    char *p = buf;
    int error;
    int current_read = READ_SIZE;
    while (true) {
        machine_int_t out_sz = o->type->stream_p.read(self_in, p, current_read, &error);
        if (out_sz == -1) {
            nlr_jump(mp_obj_new_exception_msg_varg(MP_QSTR_OSError, "[Errno %d]", error));
        }
        if (out_sz == 0) {
            break;
        }
        total_size += out_sz;
        if (out_sz < current_read) {
            current_read -= out_sz;
            p += out_sz;
        } else {
            current_read = READ_SIZE;
            p = vstr_extend(vstr, current_read);
            if (p == NULL) {
                // TODO
                nlr_jump(mp_obj_new_exception_msg_varg(MP_QSTR_OSError/*MP_QSTR_RuntimeError*/, "Out of memory"));
            }
        }
    }
    vstr_set_size(vstr, total_size + 1); // TODO: for \0
    buf[total_size] = 0;
    return mp_obj_new_str(qstr_from_str_take(buf, total_size + 1));
}

MP_DEFINE_CONST_FUN_OBJ_2(mp_stream_read_obj, stream_read);
MP_DEFINE_CONST_FUN_OBJ_1(mp_stream_readall_obj, stream_readall);
MP_DEFINE_CONST_FUN_OBJ_2(mp_stream_write_obj, stream_write);
