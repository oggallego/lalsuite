/*
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
*  along with with program; see the file COPYING. If not, write to the
*  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
*  MA  02110-1301  USA
*/

#ifndef _LIGOMETADATABURSTUTILS_H
#define _LIGOMETADATABURSTUTILS_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <lal/LIGOMetadataTables.h>

/*
 *
 * burst specific functions
 *
 */


void
XLALDestroySnglBurst(
	SnglBurst *event
);

void
XLALDestroySnglBurstTable(
	SnglBurst *head
);

int
XLALSnglBurstTableLength(
	SnglBurst *head
);

long
XLALSnglBurstAssignIDs(
	SnglBurst *head,
	long process_id,
	long event_id
);

SnglBurst **
XLALSortSnglBurst(
	SnglBurst **head,
	int (*comparefunc)(const SnglBurst * const *, const SnglBurst * const *)
);

int
XLALCompareSnglBurstByPeakTimeAndSNR(
	const SnglBurst * const *a,
	const SnglBurst * const *b
);

SnglBurst *
XLALCreateSnglBurst(
	void
);

SimBurst *
XLALCreateSimBurst(
	void
);

void
XLALDestroySimBurst(
	SimBurst *row
);

void
XLALDestroySimBurstTable(
	SimBurst *head
);

long
XLALSimBurstAssignIDs(
	SimBurst *head,
	long process_id,
	long time_slide_id,
	long event_id
);

#ifdef  __cplusplus
}
#endif

#endif /* _LIGOMETADATABURSTUTILS_H */
