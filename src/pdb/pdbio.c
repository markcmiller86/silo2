/*
Copyright (C) 1994-2016 Lawrence Livermore National Security, LLC.
LLNL-CODE-425250.
All rights reserved.

This file is part of Silo. For details, see silo.llnl.gov.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the disclaimer below.
   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the disclaimer (as noted
     below) in the documentation and/or other materials provided with
     the distribution.
   * Neither the name of the LLNS/LLNL nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

THIS SOFTWARE  IS PROVIDED BY  THE COPYRIGHT HOLDERS  AND CONTRIBUTORS
"AS  IS" AND  ANY EXPRESS  OR IMPLIED  WARRANTIES, INCLUDING,  BUT NOT
LIMITED TO, THE IMPLIED  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A  PARTICULAR  PURPOSE ARE  DISCLAIMED.  IN  NO  EVENT SHALL  LAWRENCE
LIVERMORE  NATIONAL SECURITY, LLC,  THE U.S.  DEPARTMENT OF  ENERGY OR
CONTRIBUTORS BE LIABLE FOR  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR  CONSEQUENTIAL DAMAGES  (INCLUDING, BUT NOT  LIMITED TO,
PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS  OF USE,  DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER  IN CONTRACT, STRICT LIABILITY,  OR TORT (INCLUDING
NEGLIGENCE OR  OTHERWISE) ARISING IN  ANY WAY OUT  OF THE USE  OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This work was produced at Lawrence Livermore National Laboratory under
Contract No.  DE-AC52-07NA27344 with the DOE.

Neither the  United States Government nor  Lawrence Livermore National
Security, LLC nor any of  their employees, makes any warranty, express
or  implied,  or  assumes  any  liability or  responsibility  for  the
accuracy, completeness,  or usefulness of  any information, apparatus,
product, or  process disclosed, or  represents that its use  would not
infringe privately-owned rights.

Any reference herein to  any specific commercial products, process, or
services by trade name,  trademark, manufacturer or otherwise does not
necessarily  constitute or imply  its endorsement,  recommendation, or
favoring  by  the  United  States  Government  or  Lawrence  Livermore
National Security,  LLC. The views  and opinions of  authors expressed
herein do not necessarily state  or reflect those of the United States
Government or Lawrence Livermore National Security, LLC, and shall not
be used for advertising or product endorsement purposes.
*/
/*
 * PDBIO.C - handle file I/O for PDBLib
 *         - do things so that it can work over networks and so on
 *
 * Source Version: 9.0
 * Software Release #92-0043
 *
 */
#include "config.h"
#if HAVE_STDARG_H
#include <stdarg.h>
#endif
#include "pdb.h"

static char 	Pbuffer[LRG_TXT_BUFFER];


/*-------------------------------------------------------------------------
 * Function:	_lite_PD_pio_close
 *
 * Purpose:	Close the file wherever it is.
 *
 * Return:	Success:	
 *
 *		Failure:	
 *
 * Programmer:	Adapted from PACT PDB
 *		Mar  5, 1996  2:26 PM EST
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
int
_lite_PD_pio_close (FILE *stream) {

   int ret;

   if (stream == NULL) return(EOF);
   
   ret = fclose(stream);
   return(ret);
}


/*-------------------------------------------------------------------------
 * Function:	_lite_PD_pio_seek
 *
 * Purpose:	Do an fseek on the file wherever it is.
 *
 * Return:	Success:	0
 *
 *		Failure:	nonzero
 *
 * Programmer:	Adapted from PACT PDB
 *		Mar  5, 1996  2:29 PM EST
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
int
_lite_PD_pio_seek (FILE *stream, long addr, int offset) {

   int ret;

   if (stream == NULL) return(EOF);
   
   ret = fseek(stream, addr, offset);

   return(ret);
}


/*-------------------------------------------------------------------------
 * Function:	_lite_PD_pio_printf
 *
 * Purpose:	Do an fprintf style write to the given file, wherever it is.
 *
 * Return:	Success:	
 *
 *		Failure:	
 *
 * Programmer:	Adapted from PACT PDB
 *		Mar  5, 1996  2:27 PM EST
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
int
_lite_PD_pio_printf(FILE *fp, char *fmt, ...) {

   va_list	ap ;
   size_t	ni, nw;

   va_start (ap, fmt);
   vsprintf (Pbuffer, fmt, ap);
   va_end (ap) ;

   ni = strlen(Pbuffer);
   nw = io_write(Pbuffer, (size_t) 1, ni, fp);

   return((int) nw);
}
