/*
 * Copyright (c) 2001 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#if !defined(WINNT)
#ident "$Id: vpi_signal.cc,v 1.33 2002/02/03 01:01:51 steve Exp $"
#endif

/*
 * vpiReg handles are handled here. These objects represent vectors of
 * .var objects that can be manipulated by the VPI module.
 */

# include  "vpi_priv.h"
# include  "functor.h"
# include  <stdio.h>
#ifdef HAVE_MALLOC_H
# include  <malloc.h>
#endif
# include  <stdlib.h>
# include  <string.h>
# include  <assert.h>

/*
 * Hex digits that represent 4-value bits of Verilog are not as
 * trivially obvious to display as if the bits were the usual 2-value
 * bits. So, although it is possible to write a function that
 * generates a correct character for 4*4-value bits, it is easier to
 * just perform the lookup in a table. This only takes 256 bytes,
 * which is not many executable instructions:-)
 *
 * The table is calculated as compile time, therefore, by the
 * draw_tt.c program.
 */
extern const char hex_digits[256];

extern const char oct_digits[256];

/*
 * The string values need a result buf to hold the results. This
 * buffer can be reused for that purpose. Whenever I have a need, the
 * need_result_buf function makes sure that need can be met.
 */
static char*result_buf = 0;
static size_t result_buf_size = 0;
static void need_result_buf(size_t cnt)
{
      if (result_buf_size == 0) {
	    result_buf = (char*)malloc(cnt);
      } else if (result_buf_size < cnt) {
	    result_buf = (char*)realloc(result_buf, cnt);
      }
      result_buf_size = cnt;
}

/*
 * implement vpi_get for vpiReg objects.
 */
static int signal_get(int code, vpiHandle ref)
{
      assert((ref->vpi_type->type_code==vpiNet)
	     || (ref->vpi_type->type_code==vpiReg));

      struct __vpiSignal*rfp = (struct __vpiSignal*)ref;

      switch (code) {

	  case vpiSigned:
	    return rfp->signed_flag != 0;

	  case vpiSize:
	    if (rfp->msb >= rfp->lsb)
		  return rfp->msb - rfp->lsb + 1;
	    else
		  return rfp->lsb - rfp->msb + 1;

	  case _vpiNexusId:
	    if (rfp->msb == rfp->lsb)
		  return vvp_fvector_get(rfp->bits, 0);
	    else
		  return 0;

	  default:
	    return 0;
      }
}

static char* signal_get_str(int code, vpiHandle ref)
{
      assert((ref->vpi_type->type_code==vpiNet)
	     || (ref->vpi_type->type_code==vpiReg));

      struct __vpiSignal*rfp = (struct __vpiSignal*)ref;

      static char full_name[4096];

      switch (code) {

	  case vpiFullName:
	    strcpy(full_name, vpi_get_str(vpiFullName, &rfp->scope->base));
	    strcat(full_name, ".");
	    strcat(full_name, rfp->name);
	    return full_name;

	  case vpiName:
	    return (char*)rfp->name;
      }

      return 0;
}

static vpiHandle signal_get_handle(int code, vpiHandle ref)
{
      assert((ref->vpi_type->type_code==vpiNet)
	     || (ref->vpi_type->type_code==vpiReg));

      struct __vpiSignal*rfp = (struct __vpiSignal*)ref;

      switch (code) {

	  case vpiScope:
	    return &rfp->scope->base;
      }

      return 0;
}


static void signal_vpiDecStrVal(struct __vpiSignal*rfp, s_vpi_value*vp)
{
      unsigned wid = (rfp->msb >= rfp->lsb)
	    ? (rfp->msb - rfp->lsb + 1)
	    : (rfp->lsb - rfp->msb + 1);

      unsigned char*bits = new unsigned char[wid];
      for (unsigned idx = 0 ;  idx < wid ;  idx += 1) {
	    vvp_ipoint_t fptr = vvp_fvector_get(rfp->bits, idx);
	    bits[idx] = functor_get(fptr);
      }

      need_result_buf((wid+2) / 3 + 1);

      vpip_bits_to_dec_str(bits, wid, result_buf, result_buf_size,
			   rfp->signed_flag);

      delete[]bits;
}


static void signal_vpiStringVal(struct __vpiSignal*rfp, s_vpi_value*vp)
{
      char*cp;
      unsigned wid = (rfp->msb >= rfp->lsb)
	    ? (rfp->msb - rfp->lsb + 1)
	    : (rfp->lsb - rfp->msb + 1);


      /* The result will use a character for each 8 bits of the
	 vector. Add one extra character for the highest bits that
         don't form an 8 bit group. */
      need_result_buf(wid/8 + ((wid&7)!=0) + 1);

      cp = result_buf;
      char tmp = 0;
      int bitnr;
      for(bitnr=wid-1; bitnr>=0; bitnr--){
	  vvp_ipoint_t fptr = vvp_fvector_get(rfp->bits, bitnr);
	  tmp <<= 1;

	  switch (functor_get(fptr)) {
	  case 0:
	      break;
	  case 1:
	      tmp |= 1;
	      break;
	  default:
	      break;
	  }

	  if ((bitnr&7)==0){
	      *cp++ = tmp? tmp : ' ';
	      tmp = 0;
	  }
      }
      *cp++ = 0;
}

/*
 * The get_value method reads the values of the functors and returns
 * the vector to the caller. This causes no side-effect, and reads the
 * variables like a %load would.
 */
static void signal_get_value(vpiHandle ref, s_vpi_value*vp)
{
      assert((ref->vpi_type->type_code==vpiNet)
	     || (ref->vpi_type->type_code==vpiReg));

      struct __vpiSignal*rfp = (struct __vpiSignal*)ref;

      unsigned wid = (rfp->msb >= rfp->lsb)
	    ? (rfp->msb - rfp->lsb + 1)
	    : (rfp->lsb - rfp->msb + 1);

      switch (vp->format) {

	  case vpiIntVal:
	    assert(wid <= 8 * sizeof vp->value.integer);
	    vp->value.integer = 0;
	    for (unsigned idx = 0 ;  idx < wid ;  idx += 1) {
		  vvp_ipoint_t fptr = vvp_fvector_get(rfp->bits, idx);
		  switch (functor_get(fptr)) {
		      case 0:
			break;
		      case 1:
			vp->value.integer |= 1<<idx;
			break;
		      default:
			idx = wid;
			vp->value.integer = 0;
			break;
		  }
	    }
	    break;

	  case vpiBinStrVal:
	    need_result_buf(wid+1);

	    for (unsigned idx = 0 ;  idx < wid ;  idx += 1) {
		  vvp_ipoint_t fptr = vvp_fvector_get(rfp->bits, idx);
		  result_buf[wid-idx-1] = "01xz"[functor_get(fptr)];
	    }
	    result_buf[wid] = 0;
	    vp->value.str = result_buf;
	    break;

	  case vpiHexStrVal: {
		unsigned hval, hwid;
		hwid = (wid + 3) / 4;

		need_result_buf(hwid+1);
		result_buf[hwid] = 0;
		hval = 0;
		for (unsigned idx = 0 ;  idx < wid ;  idx += 1) {
		      vvp_ipoint_t fptr = vvp_fvector_get(rfp->bits, idx);
		      hval = hval | (functor_get(fptr) << 2*(idx % 4));

		      if (idx%4 == 3) {
			    hwid -= 1;
			    result_buf[hwid] = hex_digits[hval];
			    hval = 0;
		      }
		}

		if (hwid > 0) {
		      hwid -= 1;
		      result_buf[hwid] = hex_digits[hval];
		      unsigned padd = 0;
		      switch(result_buf[hwid]) {
			  case 'X': padd = 2; break;
			  case 'Z': padd = 3; break;
		      }
		      if (padd) {
			    for (unsigned idx = wid % 4; idx < 4; idx += 1) {
				  hval = hval | padd << 2*idx;
			    }
			    result_buf[hwid] = hex_digits[hval];
		      }
		}
		vp->value.str = result_buf;
		break;
	  }

	  case vpiOctStrVal: {
		unsigned hval, hwid;
		hwid = (wid + 2) / 3;

		need_result_buf(hwid+1);
		result_buf[hwid] = 0;
		hval = 0;
		for (unsigned idx = 0 ;  idx < wid ;  idx += 1) {
		      vvp_ipoint_t fptr = vvp_fvector_get(rfp->bits, idx);
		      hval = hval | (functor_get(fptr) << 2*(idx % 3));

		      if (idx%3 == 2) {
			    hwid -= 1;
			    result_buf[hwid] = oct_digits[hval];
			    hval = 0;
		      }
		}

		if (hwid > 0) {
		      hwid -= 1;
		      result_buf[hwid] = oct_digits[hval];
		      unsigned padd = 0;
		      switch(result_buf[hwid]) {
			  case 'X': padd = 2; break;
			  case 'Z': padd = 3; break;
		      }
		      if (padd) {
			    for (unsigned idx = wid % 3; idx < 3; idx += 1) {
				  hval = hval | padd << 2*idx;
			    }
			    result_buf[hwid] = oct_digits[hval];
		      }
		}
		vp->value.str = result_buf;
		break;
	  }

	  case vpiDecStrVal:
	    signal_vpiDecStrVal(rfp, vp);
	    vp->value.str = result_buf;
	    break;

	  case vpiStringVal:
	    signal_vpiStringVal(rfp, vp);
	    vp->value.str = result_buf;
	    break;

          case vpiVectorVal:
	    {
	      s_vpi_vecval *op = vp->value.vector;
	      unsigned int obit = 0;
	      for (unsigned idx = 0 ;  idx < wid ;  idx += 1) {
		vvp_ipoint_t fptr = vvp_fvector_get(rfp->bits, idx);
		switch (functor_get(fptr)) {
		case 0:
		  op->aval &= ~(1 << obit);
		  op->bval &= ~(1 << obit);
		  break;
		case 1:
		  op->aval |= (1 << obit);
		  op->bval &= ~(1 << obit);
		  break;
		case 2:
		  op->aval &= ~(1 << obit);
		  op->bval |= (1 << obit);
		  break;
		case 3:
		  op->aval |= (1 << obit);
		  op->bval |= (1 << obit);
		  break;
		}
		obit++;
		if (obit == 8*sizeof(op->aval)) {
		  op++;
		  obit = 0;
		}
	      }
	    }
	    break;

	  default:
	    fprintf(stderr, "vvp internal error: signal_get_value: "
		    "value type %u not implemented.\n", vp->format);

	    assert(0);
      }
}

/*
 * The put_value method writes the value into the vector, and returns
 * the affected ref. This operation works much like the %set or
 * %assign instructions and causes all the side-effects that the
 * equivilent instruction would cause.
 */

static void functor_poke(struct __vpiSignal*rfp, unsigned idx, 
			 unsigned val, unsigned str)
{
      vvp_ipoint_t ptr = vvp_fvector_get(rfp->bits,idx);
      functor_t fu = functor_index(ptr);
      fu->put_ostr(val, str, true);
}

static vpiHandle signal_put_value(vpiHandle ref, s_vpi_value*vp,
				  p_vpi_time when, int flags)
{
      unsigned wid;
      struct __vpiSignal*rfp;

      assert((ref->vpi_type->type_code==vpiNet)
	     || (ref->vpi_type->type_code==vpiReg));

      rfp = (struct __vpiSignal*)ref;

	/* XXXX delays are not yet supported. */
      assert(flags == vpiNoDelay);

      wid = (rfp->msb >= rfp->lsb)
	    ? (rfp->msb - rfp->lsb + 1)
	    : (rfp->lsb - rfp->msb + 1);

      switch (vp->format) {

	  case vpiIntVal: {
		if (wid > 8*sizeof(long)) {
		      fprintf(stderr, "internal error: wid(%u) "
			      "too large.\n", wid);
		      assert(0);
		}
		
		long val = vp->value.integer;
		for (unsigned idx = 0 ;  idx < wid ;  idx += 1) {
		      functor_poke(rfp, idx, val&1, (val&1)? St1 : St0);
		      val >>= 1;
		}
		break;
	  }

	  case vpiScalarVal:
	    switch (vp->value.scalar) {
		case vpi0:
		  functor_poke(rfp, 0, 0, St0);
		  break;
		case vpi1:
		  functor_poke(rfp, 0, 1, St1);
		  break;
		case vpiX:
		  functor_poke(rfp, 0, 2, StX);
		  break;
		case vpiZ:
		  functor_poke(rfp, 0, 3, HiZ);
		  break;
		default:
		  assert(0);
	    }
	    break;

	  case vpiVectorVal: {
		assert(wid <= sizeof (unsigned long));

		unsigned long aval = vp->value.vector->aval;
		unsigned long bval = vp->value.vector->bval;
		for (unsigned idx = 0 ;  idx < wid ;  idx += 1) {
		      int bit = (aval&1) | ((bval<<1)&2);
		      switch (bit) {
			  case 0: /* zero */
			    functor_poke(rfp,idx, 0, St0);
			    break;
			  case 1: /* one */
			    functor_poke(rfp,idx, 1, St1);
			    break;
			  case 2: /* z */
			    functor_poke(rfp,idx, 3, HiZ);
			    break;
			  case 3: /* x */
			    functor_poke(rfp,idx, 2, StX);
			    break;
		      }
		      aval >>= 1;
		      bval >>= 1;
		}
		break;
	  }

	  default:
	    assert(0);

      }

      return ref;
}

static const struct __vpirt vpip_reg_rt = {
      vpiReg,
      signal_get,
      signal_get_str,
      signal_get_value,
      signal_put_value,
      signal_get_handle,
      0
};

static const struct __vpirt vpip_net_rt = {
      vpiNet,
      signal_get,
      signal_get_str,
      signal_get_value,
      signal_put_value,
      signal_get_handle,
      0
};

/*
 * Construct a vpiReg object. It's like a net, except for the type.
 */
vpiHandle vpip_make_reg(char*name, int msb, int lsb, bool signed_flag,
			vvp_fvector_t vec)
{
      vpiHandle obj = vpip_make_net(name, msb,lsb, signed_flag, vec);
      obj->vpi_type = &vpip_reg_rt;
      return obj;
}

/*
 * Construct a vpiNet object. Give the object specified dimensions,
 * and point to the specified functor for the lsb.
 */
vpiHandle vpip_make_net(char*name, int msb, int lsb, bool signed_flag,
			vvp_fvector_t vec)
{
      struct __vpiSignal*obj = (struct __vpiSignal*)
	    malloc(sizeof(struct __vpiSignal));
      obj->base.vpi_type = &vpip_net_rt;
      obj->name = name;
      obj->msb = msb;
      obj->lsb = lsb;
      obj->signed_flag = signed_flag? 1 : 0;
      obj->bits = vec;
      obj->callback = 0;

      obj->scope = vpip_peek_current_scope();

      return &obj->base;
}


/*
 * $Log: vpi_signal.cc,v $
 * Revision 1.33  2002/02/03 01:01:51  steve
 *  Use Larrys bits-to-decimal-string code.
 *
 * Revision 1.32  2002/01/09 03:29:12  steve
 *  String prints of non-round vectors (PR378)
 *
 * Revision 1.31  2001/12/18 05:31:54  steve
 *  Remove result length restrictions for vpi_get_value.
 *
 * Revision 1.30  2001/12/06 03:31:25  steve
 *  Support functor delays for gates and UDP devices.
 *  (Stephan Boettcher)
 *
 * Revision 1.29  2001/11/07 03:34:42  steve
 *  Use functor pointers where vvp_ipoint_t is unneeded.
 *
 * Revision 1.28  2001/10/31 04:27:47  steve
 *  Rewrite the functor type to have fewer functor modes,
 *  and use objects to manage the different types.
 *  (Stephan Boettcher)
 *
 * Revision 1.27  2001/10/18 04:52:31  steve
 *  Support vpiVectorVal for signals. (Philip Blundell)
 *
 * Revision 1.26  2001/10/15 01:49:50  steve
 *  Support getting scope of scope, and scope of signals.
 *
 * Revision 1.25  2001/09/30 05:18:46  steve
 *  Reduce VCD output by removing duplicates. (Stephan Boettcher)
 *
 * Revision 1.24  2001/09/15 18:27:05  steve
 *  Make configure detect malloc.h
 *
 * Revision 1.23  2001/08/09 19:38:23  steve
 *  Nets (wires) do not use their own functors.
 *  Modifications to propagation of values.
 *  (Stephan Boettcher)
 */

