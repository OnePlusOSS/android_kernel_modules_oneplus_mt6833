/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/cnm_timer.c#1
*/

/*
 * ! \file   "cnm_timer.c"
 *  \brief
 *
 */

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

#ifndef MTK_WCN_BUILT_IN_DRIVER
#include "connectivity_build_in_adapter.h"
#endif
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the time to do the time out check.
*
* \param[in] rTimeout Time out interval from current time.
*
* \retval TRUE Success.
*
*/
/*----------------------------------------------------------------------------*/
static BOOLEAN cnmTimerSetTimer(IN P_ADAPTER_T prAdapter,
				IN OS_SYSTIME rTimeout,
				IN enum ENUM_TIMER_WAKELOCK_TYPE_T eType)
{
	P_ROOT_TIMER prRootTimer;
	BOOLEAN fgNeedWakeLock;

	ASSERT(prAdapter);

	prRootTimer = &prAdapter->rRootTimer;

	kalSetTimer(prAdapter->prGlueInfo, rTimeout);

	if ((eType == TIMER_WAKELOCK_REQUEST) ||
	    (rTimeout <= SEC_TO_SYSTIME(WAKE_LOCK_MAX_TIME) && (eType == TIMER_WAKELOCK_AUTO))) {
		fgNeedWakeLock = TRUE;

		if (!prRootTimer->fgWakeLocked) {
			KAL_WAKE_LOCK(prAdapter, prRootTimer->rWakeLock);
			prRootTimer->fgWakeLocked = TRUE;
		}
	} else {
		fgNeedWakeLock = FALSE;
	}

	return fgNeedWakeLock;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routines is called to initialize a root timer.
*
* \param[in] prAdapter
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmTimerInitialize(IN P_ADAPTER_T prAdapter)
{
	P_ROOT_TIMER prRootTimer;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	prRootTimer = &prAdapter->rRootTimer;

	/* Note: glue layer have configured timer */

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TIMER);
	LINK_INITIALIZE(&prRootTimer->rLinkHead);
	prRootTimer->pucFileAndLineFor1stNode = NULL;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TIMER);

	KAL_WAKE_LOCK_INIT(prAdapter, prRootTimer->rWakeLock, "WLAN Timer");
	prRootTimer->fgWakeLocked = FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routines is called to destroy a root timer.
*        When WIFI is off, the token shall be returned back to system.
*
* \param[in]
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmTimerDestroy(IN P_ADAPTER_T prAdapter)
{
	P_ROOT_TIMER prRootTimer;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	prRootTimer = &prAdapter->rRootTimer;

	if (prRootTimer->fgWakeLocked) {
		KAL_WAKE_UNLOCK(prAdapter, prRootTimer->rWakeLock);
		prRootTimer->fgWakeLocked = FALSE;
	}
	KAL_WAKE_LOCK_DESTROY(prAdapter, prRootTimer->rWakeLock);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TIMER);
	LINK_INITIALIZE(&prRootTimer->rLinkHead);
	prRootTimer->pucFileAndLineFor1stNode = NULL;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TIMER);

	/* Note: glue layer will be responsible for timer destruction */

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routines is called to initialize a timer.
*
* \param[in] prTimer Pointer to a timer structure.
* \param[in] pfnFunc Pointer to the call back function.
* \param[in] u4Data Parameter for call back function.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
cnmTimerInitTimerOption(IN P_ADAPTER_T prAdapter,
			IN P_TIMER_T prTimer,
			IN PFN_MGMT_TIMEOUT_FUNC pfFunc,
			IN ULONG ulDataPtr,
			IN enum ENUM_TIMER_WAKELOCK_TYPE_T eType)
{
	ASSERT(prAdapter);

	ASSERT(prTimer);

	ASSERT((eType >= TIMER_WAKELOCK_AUTO) && (eType < TIMER_WAKELOCK_NUM));

#if DBG
	/* Note: NULL function pointer is permitted for HEM POWER */
	if (pfFunc == NULL)
		DBGLOG(CNM, WARN, "Init timer with NULL callback function!\n");
#endif

#if DBG
	ASSERT(prAdapter->rRootTimer.rLinkHead.prNext);
	{
		P_LINK_T prTimerList;
		P_LINK_ENTRY_T prLinkEntry;
		P_TIMER_T prPendingTimer;

		prTimerList = &(prAdapter->rRootTimer.rLinkHead);

		LINK_FOR_EACH(prLinkEntry, prTimerList) {
			prPendingTimer = LINK_ENTRY(prLinkEntry, TIMER_T, rLinkEntry);
			ASSERT(prPendingTimer);
			ASSERT(prPendingTimer != prTimer);
		}
	}
#endif

	if (prTimer->rLinkEntry.prNext) {
		DBGLOG(CNM, WARN, "re-init timer, func %p\n", pfFunc);
		/* Remove dead timer to prevent infinite loop */
		LINK_REMOVE_KNOWN_ENTRY(&prAdapter->rRootTimer.rLinkHead,
			&prTimer->rLinkEntry);
	}
	LINK_ENTRY_INITIALIZE(&prTimer->rLinkEntry);
	prTimer->pfMgmtTimeOutFunc = pfFunc;
	prTimer->ulDataPtr = ulDataPtr;
	prTimer->eType = eType;
	prTimer->pucFileAndLineForNextTimer = NULL;
	prTimer->pucFileAndLineOwn = NULL;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routines is called to stop a timer.
*
* \param[in] prTimer Pointer to a timer structure.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static VOID cnmTimerStopTimer_impl(IN P_ADAPTER_T prAdapter, IN P_TIMER_T prTimer, IN BOOLEAN fgAcquireSpinlock)
{
	P_ROOT_TIMER prRootTimer;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prTimer);

	prRootTimer = &prAdapter->rRootTimer;

	if (fgAcquireSpinlock)
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TIMER);

	if (timerPendingTimer(prTimer)) {
		if ((&prTimer->rLinkEntry)->prPrev != (P_LINK_ENTRY_T)&prRootTimer->rLinkHead) {
			P_TIMER_T prPrevTimer = LINK_ENTRY((&prTimer->rLinkEntry)->prPrev, TIMER_T, rLinkEntry);

			prPrevTimer->pucFileAndLineForNextTimer = prTimer->pucFileAndLineForNextTimer;
		} else
			prRootTimer->pucFileAndLineFor1stNode = prTimer->pucFileAndLineForNextTimer;

		LINK_REMOVE_KNOWN_ENTRY(&prRootTimer->rLinkHead, &prTimer->rLinkEntry);

		/* Reduce dummy timeout for power saving, especially HIF activity.
		 * If two or more timers exist and being removed timer is smallest,
		 * this dummy timeout will still happen, but it is OK.
		 */
		if (LINK_IS_EMPTY(&prRootTimer->rLinkHead)) {
			prRootTimer->pucFileAndLineFor1stNode = NULL;
			kalCancelTimer(prAdapter->prGlueInfo);

			if (fgAcquireSpinlock && prRootTimer->fgWakeLocked) {
				KAL_WAKE_UNLOCK(prAdapter, prRootTimer->rWakeLock);
				prRootTimer->fgWakeLocked = FALSE;
			}
		}
	}

	if (fgAcquireSpinlock)
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TIMER);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routines is called to stop a timer.
*
* \param[in] prTimer Pointer to a timer structure.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmTimerStopTimer(IN P_ADAPTER_T prAdapter, IN P_TIMER_T prTimer)
{
	ASSERT(prAdapter);
	ASSERT(prTimer);

	cnmTimerStopTimer_impl(prAdapter, prTimer, TRUE);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routines is called to start a timer with wake_lock.
*
* \param[in] prTimer Pointer to a timer structure.
* \param[in] u4TimeoutMs Timeout to issue the timer and call back function
*                        (unit: ms).
* \param[in] fileAndline Record where this func was called
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmTimerStartTimerx(IN P_ADAPTER_T prAdapter, IN P_TIMER_T prTimer, IN UINT_32 u4TimeoutMs, PUINT_8 fileAndLine)
{
	P_ROOT_TIMER prRootTimer;
	P_LINK_T prTimerList;
	OS_SYSTIME rExpiredSysTime, rTimeoutSystime;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prTimer);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TIMER);

	prRootTimer = &prAdapter->rRootTimer;
	prTimerList = &prRootTimer->rLinkHead;

	prTimer->pucFileAndLineOwn = fileAndLine;
	/* If timeout interval is larger than 1 minute, the mod value is set
	 * to the timeout value first, then per minutue.
	 */
	if (u4TimeoutMs > MSEC_PER_MIN) {
		ASSERT(u4TimeoutMs <= ((UINT_32) 0xFFFF * MSEC_PER_MIN));

		prTimer->u2Minutes = (UINT_16) (u4TimeoutMs / MSEC_PER_MIN);
		u4TimeoutMs -= (prTimer->u2Minutes * MSEC_PER_MIN);
		if (u4TimeoutMs == 0) {
			u4TimeoutMs = MSEC_PER_MIN;
			prTimer->u2Minutes--;
		}
	} else {
		prTimer->u2Minutes = 0;
	}

	/* The assertion check if MSEC_TO_SYSTIME() may be overflow. */
	ASSERT(u4TimeoutMs < (((UINT_32) 0x80000000 - MSEC_PER_SEC) / KAL_HZ));
	rTimeoutSystime = MSEC_TO_SYSTIME(u4TimeoutMs);
	if (rTimeoutSystime == 0)
		rTimeoutSystime = 1;
	rExpiredSysTime = kalGetTimeTick() + rTimeoutSystime;

	/* If no timer pending or the fast time interval is used. */
	if (LINK_IS_EMPTY(prTimerList) || TIME_BEFORE(rExpiredSysTime, prRootTimer->rNextExpiredSysTime)) {

		prRootTimer->rNextExpiredSysTime = rExpiredSysTime;
		cnmTimerSetTimer(prAdapter, rTimeoutSystime, prTimer->eType);
	}

	/* Add this timer to checking list */
	prTimer->rExpiredSysTime = rExpiredSysTime;

	if (!timerPendingTimer(prTimer)) {
		LINK_INSERT_TAIL(prTimerList, &prTimer->rLinkEntry);
		if (prRootTimer->rLinkHead.u4NumElem == 1)
			prRootTimer->pucFileAndLineFor1stNode = fileAndLine;
		else {
			struct _LINK_ENTRY_T *previous = prTimer->rLinkEntry.prPrev;
			P_TIMER_T prPrevTimer = LINK_ENTRY(previous, TIMER_T, rLinkEntry);

			prPrevTimer->pucFileAndLineForNextTimer = fileAndLine;
		}
		prTimer->pucFileAndLineForNextTimer = NULL;
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TIMER);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routines is called to check the timer list.
*
* \param[in]
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmTimerDoTimeOutCheck(IN P_ADAPTER_T prAdapter)
{
	P_ROOT_TIMER prRootTimer;
	P_LINK_T prTimerList;
	P_LINK_ENTRY_T prLinkEntry;
	P_TIMER_T prTimer;
	OS_SYSTIME rCurSysTime;
	PFN_MGMT_TIMEOUT_FUNC pfMgmtTimeOutFunc;
	ULONG ulTimeoutDataPtr;
	BOOLEAN fgNeedWakeLock;
	enum ENUM_TIMER_WAKELOCK_TYPE_T eType = TIMER_WAKELOCK_NONE;
	P_TIMER_T prPrevTimer = NULL;
	UINT_32 index = 0;
	UINT_32 u4PrevTimerNum;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	/* acquire spin lock */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TIMER);

	prRootTimer = &prAdapter->rRootTimer;
	prTimerList = &prRootTimer->rLinkHead;

	rCurSysTime = kalGetTimeTick();

	/* Set the permitted max timeout value for new one */
	prRootTimer->rNextExpiredSysTime = rCurSysTime + MGMT_MAX_TIMEOUT_INTERVAL;

	u4PrevTimerNum = prRootTimer->rLinkHead.u4NumElem;
	LINK_FOR_EACH(prLinkEntry, prTimerList) {
		if (prLinkEntry == NULL)
			break;

		prTimer = LINK_ENTRY(prLinkEntry, TIMER_T, rLinkEntry);
		index++;

		if (!timerPendingTimer(prTimer) || (index > u4PrevTimerNum)) {
			if (prPrevTimer)
				DBGLOG(CNM, WARN, "FATAL ERROR, timer out of list, file %s, prAdapter: %p\n",
				       prPrevTimer->pucFileAndLineForNextTimer, prAdapter);
			else
				DBGLOG(CNM, WARN, "FATAL ERROR, prRootTimer->timer: %s, prAdapter: %p\n",
				       prRootTimer->pucFileAndLineFor1stNode, prAdapter);
			DBGLOG(CNM, WARN, "FATAL ERROR, timer out of list, tot timer: %d, cur: %d\n",
			       prRootTimer->rLinkHead.u4NumElem,
			       index);
			DBGLOG(CNM, WARN, "prTimer %p, expired: %u, min: %d, func %p, next: %s own: %s\n",
			       prTimer,
			       prTimer->rExpiredSysTime,
			       prTimer->u2Minutes,
			       prTimer->pfMgmtTimeOutFunc,
			       prTimer->pucFileAndLineForNextTimer,
			       prTimer->pucFileAndLineOwn);

			dumpMemory32((PUINT_32)((PUINT_8)prTimer - 256), 256);
			dumpMemory32((PUINT_32)prTimer, 256);

			if (!timerPendingTimer(prTimer))
				ASSERT(0);
		}

		prPrevTimer = prTimer;
		/* Check if this entry is timeout. */
		if (!TIME_BEFORE(rCurSysTime, prTimer->rExpiredSysTime)) {
			cnmTimerStopTimer_impl(prAdapter, prTimer, FALSE);

			pfMgmtTimeOutFunc = prTimer->pfMgmtTimeOutFunc;
			ulTimeoutDataPtr = prTimer->ulDataPtr;

			if (prTimer->u2Minutes > 0) {
				prTimer->u2Minutes--;
				prTimer->rExpiredSysTime = rCurSysTime + MSEC_TO_SYSTIME(MSEC_PER_MIN);
				LINK_INSERT_TAIL(prTimerList, &prTimer->rLinkEntry);
				/* if goto here, think the timer in list is normal */
				if (prRootTimer->rLinkHead.u4NumElem == 1)
					prRootTimer->pucFileAndLineFor1stNode = prTimer->pucFileAndLineOwn;
				else {
					struct _LINK_ENTRY_T *previous = (&prTimer->rLinkEntry)->prPrev;
					P_TIMER_T prPrev = LINK_ENTRY(previous, TIMER_T, rLinkEntry);

					prPrev->pucFileAndLineForNextTimer = prTimer->pucFileAndLineOwn;
				}
				prTimer->pucFileAndLineForNextTimer = NULL;
			} else if (pfMgmtTimeOutFunc) {
				KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TIMER);
				(pfMgmtTimeOutFunc) (prAdapter, ulTimeoutDataPtr);
				KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TIMER);
			}

			/* Search entire list again because of nest del and add timers
			 * and current MGMT_TIMER could be volatile after stopped
			 */
			prLinkEntry = (P_LINK_ENTRY_T) prTimerList;
			if (prLinkEntry == NULL)
				break;

			prPrevTimer = NULL;
			prRootTimer->rNextExpiredSysTime = rCurSysTime + MGMT_MAX_TIMEOUT_INTERVAL;
		} else if (TIME_BEFORE(prTimer->rExpiredSysTime, prRootTimer->rNextExpiredSysTime)) {
			prRootTimer->rNextExpiredSysTime = prTimer->rExpiredSysTime;

			if (prTimer->eType == TIMER_WAKELOCK_REQUEST)
				eType = TIMER_WAKELOCK_REQUEST;
			else if ((eType != TIMER_WAKELOCK_REQUEST) && (prTimer->eType == TIMER_WAKELOCK_AUTO))
				eType = TIMER_WAKELOCK_AUTO;
		}
	}			/* end of for loop */

	/* Setup the prNext timeout event. It is possible the timer was already
	 * set in the above timeout callback function.
	 */
	fgNeedWakeLock = FALSE;
	if (!LINK_IS_EMPTY(prTimerList)) {
		ASSERT(TIME_AFTER(prRootTimer->rNextExpiredSysTime, rCurSysTime));

		fgNeedWakeLock = cnmTimerSetTimer(prAdapter, (OS_SYSTIME)
						  ((INT_32) prRootTimer->rNextExpiredSysTime - (INT_32) rCurSysTime),
						  eType);
	}

	if (prRootTimer->fgWakeLocked && !fgNeedWakeLock) {
		KAL_WAKE_UNLOCK(prAdapter, prRootTimer->rWakeLock);
		prRootTimer->fgWakeLocked = FALSE;
	}

	/* release spin lock */
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TIMER);
}
