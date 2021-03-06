/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 */

#include "zrtp.h"

/* Size of extra random data to approximate a uniform distribution mod n */
#define UNIFORMBYTES	8


/*============================================================================*/
/*    Bignum Shorthand Functions                                              */
/*============================================================================*/

int bnAddMod_ (struct BigNum *rslt, struct BigNum *n1, struct BigNum *mod)
{
	bnAdd (rslt, n1);
	if (bnCmp (rslt, mod) >= 0) {
		bnSub (rslt, mod);
	}
	return 0;
}

int bnAddQMod_ (struct BigNum *rslt, unsigned n1, struct BigNum *mod)
{
	bnAddQ (rslt, n1);
	if (bnCmp (rslt, mod) >= 0) {
		bnSub (rslt, mod);
	}
	return 0;
}

int bnSubMod_ (struct BigNum *rslt, struct BigNum *n1, struct BigNum *mod)
{
	if (bnCmp (rslt, n1) < 0) {
		bnAdd (rslt, mod);
	}
	bnSub (rslt, n1);
	return 0;
}

int bnSubQMod_ (struct BigNum *rslt, unsigned n1, struct BigNum *mod)
{
	if (bnCmpQ (rslt, n1) < 0) {
		bnAdd (rslt, mod);
	}
	bnSubQ (rslt, n1);
	return 0;
}

int bnMulMod_ (struct BigNum *rslt, struct BigNum *n1, struct BigNum *n2, struct BigNum *mod)
{
	bnMul (rslt, n1, n2);
	bnMod (rslt, rslt, mod);
	return 0;
}

int bnMulQMod_ (struct BigNum *rslt, struct BigNum *n1, unsigned n2, struct BigNum *mod)
{
	bnMulQ (rslt, n1, n2);
	bnMod (rslt, rslt, mod);
	return 0;
}

int bnSquareMod_ (struct BigNum *rslt, struct BigNum *n1, struct BigNum *mod)
{
	bnSquare (rslt, n1);
	bnMod (rslt, rslt, mod);
	return 0;
}


/*============================================================================*/
/*    Elliptic Curve arithmetic                                               */
/*============================================================================*/

/* Add two elliptic curve points. Any of them may be the same object. */
int zrtp_ecAdd ( struct BigNum *rsltx, struct BigNum *rslty,
				 struct BigNum *p1x, struct BigNum *p1y,
				 struct BigNum *p2x, struct BigNum *p2y, struct BigNum *mod)
{
	struct BigNum trsltx, trslty;
	struct BigNum t1, gam;
	struct BigNum bnzero;

	bnBegin (&bnzero);

	/* Check for an operand being zero */
	if (bnCmp (p1x, &bnzero) == 0 && bnCmp (p1y, &bnzero) == 0) {
		bnCopy (rsltx, p2x); bnCopy (rslty, p2y);
		bnEnd (&bnzero);
		return 0;
	}
	if (bnCmp (p2x, &bnzero) == 0 && bnCmp (p2y, &bnzero) == 0) {
		bnCopy (rsltx, p1x); bnCopy (rslty, p1y);
		bnEnd (&bnzero);
		return 0;
	}

	/* Check if p1 == -p2 and return 0 if so */
	if (bnCmp (p1x, p2x) == 0) {
		struct BigNum tsum;
		bnBegin (&tsum);
		bnCopy (&tsum, p1x);
		bnAddMod_ (&tsum, p2x, mod);
		if (bnCmp (&tsum, &bnzero) == 0) {
			bnSetQ (rsltx, 0); bnSetQ (rslty, 0);
			bnEnd (&tsum);
			bnEnd (&bnzero);
			return 0;
		}
		bnEnd (&tsum);
	}

	bnBegin (&t1);
	bnBegin (&gam);
	bnBegin (&trsltx);
	bnBegin (&trslty);

	/* Check for doubling, different formula for gamma */
	if (bnCmp (p1x, p2x) == 0 && bnCmp (p1y, p2y) == 0) {
		bnCopy (&t1, p1y);
		bnAddMod_ (&t1, p1y, mod);
		bnInv (&t1, &t1, mod);
		bnSquareMod_ (&gam, p1x, mod);
		bnMulQMod_ (&gam, &gam, 3, mod);
		bnSubQMod_ (&gam, 3, mod);
		bnMulMod_ (&gam, &gam, &t1, mod);
	} else {
		bnCopy (&t1, p2x);
		bnSubMod_ (&t1, p1x, mod);
		bnInv (&t1, &t1, mod);
		bnCopy (&gam, p2y);
		bnSubMod_ (&gam, p1y, mod);
		bnMulMod_ (&gam, &gam, &t1, mod);
	}

	bnSquareMod_ (&trsltx, &gam, mod);
	bnSubMod_ (&trsltx, p1x, mod);
	bnSubMod_ (&trsltx, p2x, mod);

	bnCopy (&trslty, p1x);
	bnSubMod_ (&trslty, &trsltx, mod);
	bnMulMod_ (&trslty, &trslty, &gam, mod);
	bnSubMod_ (&trslty, p1y, mod);

	bnCopy (rsltx, &trsltx);
	bnCopy (rslty, &trslty);

	bnEnd (&t1);
	bnEnd (&gam);
	bnEnd (&trsltx);
	bnEnd (&trslty);
	bnEnd (&bnzero);

	return 0;
}

int zrtp_ecMul ( struct BigNum *rsltx, struct BigNum *rslty, struct BigNum *mult,
				 struct BigNum *basex, struct BigNum *basey, struct BigNum *mod)
{
	struct BigNum bnzero;
	struct BigNum tbasex, tbasey;
	struct BigNum trsltx, trslty;
	struct BigNum tmult;

	bnBegin (&bnzero);
	bnBegin (&tbasex);
	bnBegin (&tbasey);
	bnBegin (&trsltx);
	bnBegin (&trslty);
	bnBegin (&tmult);

	/* Initialize result to 0 before additions */
	bnSetQ (&trsltx, 0);
	bnSetQ (&trslty, 0);
	/* Make copies of base and multiplier */
	bnCopy (&tbasex, basex);
	bnCopy (&tbasey, basey);
	bnCopy (&tmult, mult);
	while (bnCmp (&tmult, &bnzero) > 0) {
		/* Test lsb of mult */
		unsigned lsw = bnLSWord (&tmult);
		if (lsw & 1) {
			/* Add base to result */
			zrtp_ecAdd (&trsltx, &trslty, &trsltx, &trslty, &tbasex, &tbasey, mod);
		}
		/* Double the base */
		zrtp_ecAdd (&tbasex, &tbasey, &tbasex, &tbasey, &tbasex, &tbasey, mod);
		/* Shift multiplier right */
		bnRShift (&tmult, 1);
	}

	bnCopy (rsltx, &trsltx);
	bnCopy (rslty, &trslty);

	bnEnd (&bnzero);
	bnEnd (&tbasex);
	bnEnd (&tbasey);
	bnEnd (&trsltx);
	bnEnd (&trslty);
	bnEnd (&tmult);
	return 0;
}



/*----------------------------------------------------------------------------*/
/* Choose a random point on the elliptic curve.                               */
/* Provision is made to use a given point from test vectors.                  */
/* pkx and pky are the output point, sv is output discrete log                */
/* Input base is Gx, Gy; curve field modulus is P; curve order is n.          */
/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_ec_random_point(	zrtp_global_t *zrtp,
									struct BigNum *P,
									struct BigNum *n,
									struct BigNum *Gx,
									struct BigNum *Gy,									
									struct BigNum *pkx,
									struct BigNum *pky,
									struct BigNum *sv,
									uint8_t *test_sv_data,
								    size_t test_sv_data_len)
{
	zrtp_status_t s = zrtp_status_fail;
	unsigned char* buffer = zrtp_sys_alloc(sizeof(zrtp_uchar1024_t));	
	
	if (!buffer) {
		return zrtp_status_alloc_fail;
	}
	zrtp_memset(buffer, 0, sizeof(zrtp_uchar1024_t));
	
	do
	{		
		if (test_sv_data_len != 0) {
			/* Force certain secret value */
			if (bnBytes(P) != test_sv_data_len) {			
				break;
			}
			zrtp_memcpy(buffer+UNIFORMBYTES, test_sv_data, test_sv_data_len);
		} else {
			/* Choose random value, larger than needed so it will be uniform */
			if (bnBytes(P)+UNIFORMBYTES != (uint32_t)zrtp_randstr(zrtp, buffer, bnBytes(P)+UNIFORMBYTES)) {
				break; /* if we can't generate random string - fail initialization */
			}
		}

		bnInsertBigBytes(sv, (const unsigned char *)buffer, 0, bnBytes(P)+UNIFORMBYTES);
		bnMod(sv, sv, n);
		zrtp_ecMul(pkx, pky, sv, Gx, Gy, P);

		s = zrtp_status_ok;
	} while (0);

	if (buffer) {
		zrtp_sys_free(buffer);
	}
	
	return s;
}


/*============================================================================*/
/*    Curve parameters                                                        */
/*============================================================================*/

uint8_t P_256_data[] =
{
	0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

uint8_t n_256_data[] =
{
	0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84,
	0xF3, 0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51
};

uint8_t b_256_data[] =
{
	0x5a, 0xc6, 0x35, 0xd8, 0xaa, 0x3a, 0x93, 0xe7,
	0xb3, 0xeb, 0xbd, 0x55, 0x76, 0x98, 0x86, 0xbc,
	0x65, 0x1d, 0x06, 0xb0, 0xcc, 0x53, 0xb0, 0xf6,
	0x3b, 0xce, 0x3c, 0x3e, 0x27, 0xd2, 0x60, 0x4b
};

uint8_t Gx_256_data[] =
{
	0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47,
	0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
	0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0,
	0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96
};

uint8_t Gy_256_data[] =
{
	0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b,
	0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
	0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce,
	0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5
};



uint8_t P_384_data[] =
{
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
	0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF
};

uint8_t n_384_data[] =
{
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xC7, 0x63, 0x4D, 0x81, 0xF4, 0x37, 0x2D, 0xDF,
	0x58, 0x1A, 0x0D, 0xB2, 0x48, 0xB0, 0xA7, 0x7A,
	0xEC, 0xEC, 0x19, 0x6A, 0xCC, 0xC5, 0x29, 0x73
};

uint8_t b_384_data[] =
{
	0xb3, 0x31, 0x2f, 0xa7, 0xe2, 0x3e, 0xe7, 0xe4,
	0x98, 0x8e, 0x05, 0x6b, 0xe3, 0xf8, 0x2d, 0x19,
	0x18, 0x1d, 0x9c, 0x6e, 0xfe, 0x81, 0x41, 0x12,
	0x03, 0x14, 0x08, 0x8f, 0x50, 0x13, 0x87, 0x5a,
	0xc6, 0x56, 0x39, 0x8d, 0x8a, 0x2e, 0xd1, 0x9d,
	0x2a, 0x85, 0xc8, 0xed, 0xd3, 0xec, 0x2a, 0xef
};

uint8_t Gx_384_data[] =
{
	0xaa, 0x87, 0xca, 0x22, 0xbe, 0x8b, 0x05, 0x37,
	0x8e, 0xb1, 0xc7, 0x1e, 0xf3, 0x20, 0xad, 0x74,
	0x6e, 0x1d, 0x3b, 0x62, 0x8b, 0xa7, 0x9b, 0x98,
	0x59, 0xf7, 0x41, 0xe0, 0x82, 0x54, 0x2a, 0x38,
	0x55, 0x02, 0xf2, 0x5d, 0xbf, 0x55, 0x29, 0x6c,
	0x3a, 0x54, 0x5e, 0x38, 0x72, 0x76, 0x0a, 0xb7
};

uint8_t Gy_384_data[] =
{
	0x36, 0x17, 0xde, 0x4a, 0x96, 0x26, 0x2c, 0x6f,
	0x5d, 0x9e, 0x98, 0xbf, 0x92, 0x92, 0xdc, 0x29,
	0xf8, 0xf4, 0x1d, 0xbd, 0x28, 0x9a, 0x14, 0x7c,
	0xe9, 0xda, 0x31, 0x13, 0xb5, 0xf0, 0xb8, 0xc0,
	0x0a, 0x60, 0xb1, 0xce, 0x1d, 0x7e, 0x81, 0x9d,
	0x7a, 0x43, 0x1d, 0x7c, 0x90, 0xea, 0x0e, 0x5f
};


uint8_t P_521_data[] =
{
	0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF
};

uint8_t n_521_data[] =
{
	0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFA, 0x51, 0x86, 0x87, 0x83, 0xBF, 0x2F,
	0x96, 0x6B, 0x7F, 0xCC, 0x01, 0x48, 0xF7, 0x09,
	0xA5, 0xD0, 0x3B, 0xB5, 0xC9, 0xB8, 0x89, 0x9C,
	0x47, 0xAE, 0xBB, 0x6F, 0xB7, 0x1E, 0x91, 0x38,
	0x64, 0x09
};

uint8_t b_521_data[] =
{
	0x00, 0x51, 0x95, 0x3e, 0xb9, 0x61, 0x8e, 0x1c,
	0x9a, 0x1f, 0x92, 0x9a, 0x21, 0xa0, 0xb6, 0x85,
	0x40, 0xee, 0xa2, 0xda, 0x72, 0x5b, 0x99, 0xb3,
	0x15, 0xf3, 0xb8, 0xb4, 0x89, 0x91, 0x8e, 0xf1,
	0x09, 0xe1, 0x56, 0x19, 0x39, 0x51, 0xec, 0x7e,
	0x93, 0x7b, 0x16, 0x52, 0xc0, 0xbd, 0x3b, 0xb1,
	0xbf, 0x07, 0x35, 0x73, 0xdf, 0x88, 0x3d, 0x2c,
	0x34, 0xf1, 0xef, 0x45, 0x1f, 0xd4, 0x6b, 0x50,
	0x3f, 0x00
};

uint8_t Gx_521_data[] =
{
	0x00, 0xc6, 0x85, 0x8e, 0x06, 0xb7, 0x04, 0x04,
	0xe9, 0xcd, 0x9e, 0x3e, 0xcb, 0x66, 0x23, 0x95,
	0xb4, 0x42, 0x9c, 0x64, 0x81, 0x39, 0x05, 0x3f,
	0xb5, 0x21, 0xf8, 0x28, 0xaf, 0x60, 0x6b, 0x4d,
	0x3d, 0xba, 0xa1, 0x4b, 0x5e, 0x77, 0xef, 0xe7,
	0x59, 0x28, 0xfe, 0x1d, 0xc1, 0x27, 0xa2, 0xff,
	0xa8, 0xde, 0x33, 0x48, 0xb3, 0xc1, 0x85, 0x6a,
	0x42, 0x9b, 0xf9, 0x7e, 0x7e, 0x31, 0xc2, 0xe5,
	0xbd, 0x66
};

uint8_t Gy_521_data[] =
{
	0x01, 0x18, 0x39, 0x29, 0x6a, 0x78, 0x9a, 0x3b,
	0xc0, 0x04, 0x5c, 0x8a, 0x5f, 0xb4, 0x2c, 0x7d,
	0x1b, 0xd9, 0x98, 0xf5, 0x44, 0x49, 0x57, 0x9b,
	0x44, 0x68, 0x17, 0xaf, 0xbd, 0x17, 0x27, 0x3e,
	0x66, 0x2c, 0x97, 0xee, 0x72, 0x99, 0x5e, 0xf4,
	0x26, 0x40, 0xc5, 0x50, 0xb9, 0x01, 0x3f, 0xad,
	0x07, 0x61, 0x35, 0x3c, 0x70, 0x86, 0xa2, 0x72,
	0xc2, 0x40, 0x88, 0xbe, 0x94, 0x76, 0x9f, 0xd1,
	0x66, 0x50
};

/*----------------------------------------------------------------------------*/
/* Initialize the curve parameters struct                                     */
zrtp_status_t zrtp_ec_init_params( struct zrtp_ec_params *params, uint32_t bits )
{
    unsigned ec_bytes = (bits+7) / 8;
	params->ec_bits = bits;
	switch (bits) {
	case 256:
		zrtp_memcpy (params->P_data, P_256_data, ec_bytes);
		zrtp_memcpy (params->n_data, n_256_data, ec_bytes);
		zrtp_memcpy (params->b_data, b_256_data, ec_bytes);
		zrtp_memcpy (params->Gx_data, Gx_256_data, ec_bytes);
		zrtp_memcpy (params->Gy_data, Gy_256_data, ec_bytes);
		break;
	case 384:
		zrtp_memcpy (params->P_data, P_384_data, ec_bytes);
		zrtp_memcpy (params->n_data, n_384_data, ec_bytes);
		zrtp_memcpy (params->b_data, b_384_data, ec_bytes);
		zrtp_memcpy (params->Gx_data, Gx_384_data, ec_bytes);
		zrtp_memcpy (params->Gy_data, Gy_384_data, ec_bytes);
		break;
	case 521:
		zrtp_memcpy (params->P_data, P_521_data, ec_bytes);
		zrtp_memcpy (params->n_data, n_521_data, ec_bytes);
		zrtp_memcpy (params->b_data, b_521_data, ec_bytes);
		zrtp_memcpy (params->Gx_data, Gx_521_data, ec_bytes);
		zrtp_memcpy (params->Gy_data, Gy_521_data, ec_bytes);
		break;
	default:
		return zrtp_status_bad_param;
	}
	
	return zrtp_status_ok;
}
