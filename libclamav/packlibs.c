/*
 *  Copyright (C) 2006 aCaB <acab@clamav.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */


#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif

#include "others.h"

static int doubledl(char **scur, uint8_t *mydlptr, char *buffer, uint32_t buffersize)
{
  unsigned char mydl = *mydlptr;
  unsigned char olddl = mydl;

  mydl*=2;
  if ( !(olddl & 0x7f)) {
    if ( *scur < buffer || *scur >= buffer+buffersize-1 )
      return -1;
    olddl = **scur;
    mydl = olddl*2+1;
    *scur=*scur + 1;
  }
  *mydlptr = mydl;
  return (olddl>>7)&1;
}


int cli_unfsg(char *source, char *dest, int ssize, int dsize, char **endsrc, char **enddst) {
  uint8_t mydl=0x80;
  uint32_t backbytes, backsize, oldback = 0;
  char *csrc = source, *cdst = dest;
  int oob, lostbit = 1;

  /* I assume buffers size is >0 - No checking! */
  *cdst++=*csrc++;

  while ( 1 ) {
    if ((oob=doubledl(&csrc, &mydl, source, ssize))) {
      if (oob == -1)
	return -1;
      /* 164 */
      backsize = 0;
      if ((oob=doubledl(&csrc, &mydl, source, ssize))) {
	if (oob == -1)
	  return -1;
	/* 16a */
	backbytes = 0;
	if ((oob=doubledl(&csrc, &mydl, source, ssize))) {
	  if (oob == -1)
	    return -1;
	  /* 170 */
	  lostbit = 1;
	  backsize++;
	  backbytes = 0x10;
	  while ( backbytes < 0x100 ) {
	    if ((oob=doubledl(&csrc, &mydl, source, ssize)) == -1)
	      return -1;
	    backbytes = backbytes*2+oob;
	  }
	  backbytes &= 0xff;
	  if ( ! backbytes ) {
	    if (cdst >= dest+dsize)
	      return -1;
	    *cdst++=0x00;
	    continue;
	  }
	} else {
	  /* 18f */
	  if (csrc >= source+ssize)
	    return -1;
	  backbytes = *(unsigned char*)csrc;
	  backsize = backsize * 2 + (backbytes & 1);
	  backbytes = (backbytes & 0xff)>>1;
	  csrc++;
	  if (! backbytes)
	    break;
	  backsize+=2;
	  oldback = backbytes;
	  lostbit = 0;
	}
      } else {
	/* 180 */
	backsize = 1;
	do {
	  if ((oob=doubledl(&csrc, &mydl, source, ssize)) == -1)
	    return -1;
	  backsize = backsize*2+oob;
	  if ((oob=doubledl(&csrc, &mydl, source, ssize)) == -1)
	    return -1;
	} while (oob);

	backsize = backsize - 1 - lostbit;
	if (! backsize) {
	  /* 18a */
	  backsize = 1;
	  do {
	    if ((oob=doubledl(&csrc, &mydl, source, ssize)) == -1)
	      return -1;
	    backsize = backsize*2+oob;
	    if ((oob=doubledl(&csrc, &mydl, source, ssize)) == -1)
	      return -1;
	  } while (oob);

	  backbytes = oldback;
	} else {
	  /* 198 */
	  if (csrc >= source+ssize)
	    return -1;
	  backbytes = *(unsigned char*)csrc;
	  backbytes += (backsize-1)<<8;
	  backsize = 1;
	  csrc++;
	  do {
	    if ((oob=doubledl(&csrc, &mydl, source, ssize)) == -1)
	      return -1;
	    backsize = backsize*2+oob;
	    if ((oob=doubledl(&csrc, &mydl, source, ssize)) == -1)
	      return -1;
	  } while (oob);

          if (backbytes >= 0x7d00)
            backsize++;
          if (backbytes >= 0x500)
            backsize++;
          if (backbytes <= 0x7f)
            backsize += 2;

	  oldback = backbytes;
	}
	lostbit = 0;
      }
      if (!CLI_ISCONTAINED(dest, dsize, cdst, backsize) || !CLI_ISCONTAINED(dest, dsize, cdst-backbytes, backsize))
	return -1;
      while(backsize--) {
	*cdst=*(cdst-backbytes);
	cdst++;
      }

    } else {
      /* 15d */
      if (cdst < dest || cdst >= dest+dsize || csrc < source || csrc >= source+ssize)
	return -1;
      *cdst++=*csrc++;
      lostbit=1;
    }
  }

  if (endsrc) *endsrc = csrc;
  if (enddst) *enddst = cdst;
  return 0;
}
