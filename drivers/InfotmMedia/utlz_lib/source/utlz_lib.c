/***************************************************************************** 
** 
** Copyright (c) 2012~2112 ShangHai Infotm Ltd all rights reserved. 
** 
** Use of Infotm's code is governed by terms and conditions 
** stated in the accompanying licensing statement. 
** 
**      
** Revision History: 
** ----------------- 
** v1.0.1	leo@2012/05/04: first commit.
**
*****************************************************************************/

#include <InfotmMedia.h>
#include <IM_utlzapi.h>
#include <utlz_pwl.h>
#include <utlz_lib.h>

#define DBGINFO		0
#define DBGWARN		1
#define DBGERR		1
#define DBGTIP		1

#define INFOHEAD	"UTLZLIB_I:"
#define WARNHEAD	"UTLZLIB_W:"
#define ERRHEAD		"UTLZLIB_E:"
#define TIPHEAD		"UTLZLIB_T:"

//
//
//
/* Define how often to calculate and report the utilization, in miliseconds */
#define UTLZ_STATC_TIMEOUT 1000 

//
//
//
#define UM_STATC_STAT_UNKNOWN	0
#define UM_STATC_STAT_INIT	1
#define UM_STATC_STAT_RUN	2
#define UM_STATC_STAT_STOP	3

//
typedef struct {
	utlz_module_t	module;
	IM_INT32	stat;
    IM_INT32    count;
	IM_INT64	work_start_time;
	IM_INT64	accumulated_work_time;
    IM_INT64    period_start_time;
	IM_UINT32	coeffi;
}utlz_module_statc_t;

typedef struct{
	utlz_module_statc_t	mdlstatc[UTLZ_MODULE_MAX];
	utlzpwl_lock_t		lock;
	utlzpwl_timer_t		timer;
	IM_INT32		timer_running;

	im_mempool_handle_t	mpl;
	im_list_handle_t	umlst;
}utlz_instance_t;

static utlz_instance_t	*gUtlz = IM_NULL;


//
//
//
static void calculate_modules_utilization(void* arg)
{
	utlz_module_statc_t *umblk;
	IM_INT64 time_now;
	IM_INT64 time_period;
	IM_UINT32 leading_zeros;
	IM_UINT32 shift_val;
	IM_UINT32 work_normalized;
	IM_UINT32 period_normalized;
	IM_INFOMSG((IM_STR("%s()"), IM_STR(_IM_FUNC_)));

	utlzpwl_lock(&gUtlz->lock);
	time_now = utlzpwl_get_time();
	if(gUtlz->umlst != IM_NULL)
	{
		umblk = (utlz_module_statc_t *)im_list_begin(gUtlz->umlst);
		while(umblk != IM_NULL)
		{
			if(umblk->stat == UM_STATC_STAT_UNKNOWN)
            {
                umblk = (utlz_module_statc_t *)im_list_next(gUtlz->umlst);
				continue;
            }

            umblk->count++;
            if(umblk->count < umblk->module.period)
            {
                if(umblk->period_start_time == 0)
                {
                    umblk->period_start_time = time_now;
                }
            }
            else
            {
                if(umblk->accumulated_work_time==0 && umblk->work_start_time==0)
                {
                    umblk->coeffi = 0;
                    umblk->count = 0;
                    umblk->period_start_time = time_now;
                    umblk->module.callback(umblk->coeffi);
                    umblk = (utlz_module_statc_t *)im_list_next(gUtlz->umlst);
                    continue;
                }

                time_period = time_now - umblk->period_start_time;
                
                if(umblk->work_start_time != 0)
                {
                    umblk->accumulated_work_time += (time_now - umblk->work_start_time);
                    umblk->work_start_time = time_now;
                }
                
                leading_zeros = utlzpwl_clz((IM_INT32)(time_period >> 32));
                shift_val = 32 - leading_zeros;
                work_normalized = (IM_INT32)(umblk->accumulated_work_time >> shift_val);
                period_normalized = (IM_INT32)(time_period >> shift_val);
                
                if(period_normalized > 0x00ffffff)
                {
                    period_normalized >>= 8;
                }
                else
                {
                    work_normalized <<= 8;
                }
                
                IM_INFOMSG((IM_STR("name(%s): work=%d, period=%d"), umblk->module.name, work_normalized, period_normalized));
                umblk->coeffi = work_normalized / period_normalized;
                umblk->accumulated_work_time = 0;
                umblk->module.callback(umblk->coeffi);
                umblk->period_start_time = time_now;
                umblk->count = 0;
            }
            umblk = (utlz_module_statc_t *)im_list_next(gUtlz->umlst);
        }
	}
	utlzpwl_unlock(&gUtlz->lock);
	utlzpwl_timer_add(gUtlz->timer, utlzpwl_mstoticks(UTLZ_STATC_TIMEOUT));
}

IM_RET utlzlib_init(void)
{
	IM_INFOMSG((IM_STR("%s()"), IM_STR(_IM_FUNC_)));
	IM_ASSERT(gUtlz == IM_NULL);

	if(utlzpwl_init() != IM_RET_OK){
		IM_ERRMSG((IM_STR("utlzpwl_init() failed")));
		return IM_RET_FAILED;
	}

	gUtlz = utlzpwl_malloc(sizeof(utlz_instance_t));
	if(gUtlz == IM_NULL){
		IM_ERRMSG((IM_STR("malloc(gUtlz) failed")));
		goto Fail;
	}
	utlzpwl_memset((void *)gUtlz, 0, sizeof(utlz_instance_t));

	if(utlzpwl_lock_init(&gUtlz->lock) != IM_RET_OK){
		IM_ERRMSG((IM_STR("utlzpwl_lock_init() failed")));
		goto Fail;
	}
	if(utlzpwl_timer_init(&gUtlz->timer) != IM_RET_OK){
		IM_ERRMSG((IM_STR("utlzpwl_timer_init() failed")));
		goto Fail;
	}

	utlzpwl_timer_setcallback(gUtlz->timer, calculate_modules_utilization, NULL);

	gUtlz->mpl = im_mpool_init((func_mempool_malloc_t)utlzpwl_malloc, (func_mempool_free_t)utlzpwl_free);
	if(gUtlz->mpl == IM_NULL){
		IM_ERRMSG((IM_STR("im_mpool_init() failed in %s"), IM_STR(_IM_FUNC_)));
		goto Fail;
	}

	gUtlz->umlst = im_list_init(0, gUtlz->mpl);
	if(gUtlz->umlst == IM_NULL) {
		IM_ERRMSG((IM_STR("im_list_init() failed in %s"), IM_STR(_IM_FUNC_)));
		goto Fail;
	}

	return IM_RET_OK;
Fail:
	utlzlib_deinit();
	return IM_RET_FAILED;
}

void utlzlib_suspend(void)
{
    utlz_module_statc_t *umblk;
	IM_INFOMSG((IM_STR("%s()"), IM_STR(_IM_FUNC_)));

	if(&gUtlz->timer != IM_NULL){
		utlzpwl_timer_del(&gUtlz->timer);
		gUtlz->timer_running = 0;
	}
    if(gUtlz->umlst != IM_NULL) {
        umblk = (utlz_module_statc_t *)im_list_begin(gUtlz->umlst);
        while(umblk != IM_NULL)
        {
            umblk->stat = UM_STATC_STAT_INIT;
            umblk->count = -1;
            umblk->work_start_time = 0;
            umblk->accumulated_work_time = 0;
            umblk->period_start_time = 0;
            umblk = (utlz_module_statc_t *)im_list_next(gUtlz->umlst);
        }
    }
}

IM_RET utlzlib_deinit(void)
{
	utlz_module_statc_t *umblk;
	IM_INFOMSG((IM_STR("%s()"), IM_STR(_IM_FUNC_)));

	if(gUtlz->umlst != IM_NULL) {
		umblk = (utlz_module_statc_t *)im_list_begin(gUtlz->umlst);
		while(umblk != IM_NULL)
		{
			umblk = (utlz_module_statc_t *)im_list_erase(gUtlz->umlst, (void*)umblk);
		}
		im_list_deinit(gUtlz->umlst);
	}
	if(gUtlz->mpl != IM_NULL) {
		im_mpool_deinit(gUtlz->mpl);
		gUtlz->mpl = IM_NULL;
	}

	if(gUtlz->timer != IM_NULL){
		utlzpwl_timer_del(&gUtlz->timer);
		gUtlz->timer_running = 0;
		utlzpwl_timer_term(&gUtlz->timer);
		gUtlz->timer = IM_NULL;
	}
		if(utlzpwl_lock_deinit(&gUtlz->lock) != IM_RET_OK){
			IM_ERRMSG((IM_STR("utlzpwl_lock_deinit() failed")));
		}
	utlzpwl_free((void *)gUtlz);
	gUtlz = IM_NULL;
	
	return IM_RET_OK;
}

utlz_handle_t utlzlib_register_module(utlz_module_t *um)
{
	utlz_module_statc_t *mdlstatc = utlzpwl_malloc(sizeof(utlz_module_statc_t));
	utlz_module_statc_t *umblk;
	IM_INFOMSG((IM_STR("%s(name=%s, period=%d)"), IM_STR(_IM_FUNC_), um->name, um->period));
	IM_ASSERT(gUtlz != IM_NULL);

	if(um->callback == IM_NULL){
		IM_ERRMSG((IM_STR("invalid parameter, um->callback is null")));
		goto failed;
	}

	utlzpwl_memset((void *)mdlstatc, 0, sizeof(utlz_module_statc_t));
	utlzpwl_memcpy((void *)mdlstatc, (void *)um, sizeof(utlz_module_t));
	utlzpwl_lock(&gUtlz->lock);
	mdlstatc->stat = UM_STATC_STAT_INIT;
    mdlstatc->count = -1;
    mdlstatc->period_start_time = 0;

    // check if that the module has been registered before
	umblk = (utlz_module_statc_t *)im_list_begin(gUtlz->umlst);
	while(umblk != IM_NULL)
	{
		if(!utlzpwl_strcmp(umblk->module.name, um->name))
		{
			IM_ERRMSG((IM_STR("Module %s has been registered, you should not register it again"),um->name));
			utlzpwl_unlock(&gUtlz->lock);
			goto failed;
		}
		umblk = (utlz_module_statc_t *)im_list_next(gUtlz->umlst);
	}

    // add module to the end of list
	if(im_list_put_back(gUtlz->umlst, mdlstatc) != IM_RET_OK)
	{
		IM_ERRMSG((IM_STR("failed to add module %s into list"), um->name));
		utlzpwl_unlock(&gUtlz->lock);
		goto failed;
	}

    // start calculating timeer
    if(gUtlz->timer_running == 0)
    {
        gUtlz->timer_running = 1;
        utlzpwl_timer_add(gUtlz->timer, utlzpwl_mstoticks(UTLZ_STATC_TIMEOUT));
    }

	utlzpwl_unlock(&gUtlz->lock);
	return (utlz_handle_t)mdlstatc;
failed:
	utlzpwl_free(mdlstatc);
	return IM_NULL;
}

IM_RET utlzlib_unregister_module(utlz_handle_t hdl)
{
	utlz_module_statc_t *mdlstatc = (utlz_module_statc_t *)hdl;
	IM_INFOMSG((IM_STR("%s()"), IM_STR(_IM_FUNC_)));
	IM_ASSERT(gUtlz != IM_NULL);
	IM_ASSERT(hdl != IM_NULL);

	utlzpwl_lock(&gUtlz->lock);
	mdlstatc->stat = UM_STATC_STAT_UNKNOWN;
	im_list_erase(gUtlz->umlst, (void*)mdlstatc);
	utlzpwl_unlock(&gUtlz->lock);
	utlzpwl_free(mdlstatc);

	return IM_RET_OK;
}

IM_RET utlzlib_notify(utlz_handle_t hdl, IM_INT32 code)
{
	utlz_module_statc_t *mdlstatc = (utlz_module_statc_t *)hdl;
	IM_INT64 time_now = utlzpwl_get_time();
	IM_INFOMSG((IM_STR("%s(name=%s, code=%d)"), IM_STR(_IM_FUNC_), mdlstatc->module.name, code));
	IM_ASSERT(gUtlz != IM_NULL);
	IM_ASSERT(hdl != IM_NULL);
	IM_ASSERT(mdlstatc->stat != UM_STATC_STAT_UNKNOWN);

	utlzpwl_lock(&gUtlz->lock);
	switch(code){
		case UTLZ_NOTIFY_CODE_RUN:
            if(mdlstatc->stat != UM_STATC_STAT_RUN)
            {
                mdlstatc->stat = UM_STATC_STAT_RUN;
                if(time_now < mdlstatc->period_start_time){
                    time_now = mdlstatc->period_start_time;
                }

                mdlstatc->work_start_time = time_now;
                if(gUtlz->timer_running == 0)
                {
                    gUtlz->timer_running = 1;
                    mdlstatc->count = 0;
                    mdlstatc->period_start_time = mdlstatc->work_start_time;

                    utlzpwl_timer_add(gUtlz->timer, utlzpwl_mstoticks(UTLZ_STATC_TIMEOUT));
                }
            }
            break;
        case UTLZ_NOTIFY_CODE_STOP:
            if(mdlstatc->stat != UM_STATC_STAT_STOP)
            {
                mdlstatc->stat = UM_STATC_STAT_STOP;
                if(time_now < mdlstatc->work_start_time){
                    time_now = mdlstatc->work_start_time;
                }
                mdlstatc->accumulated_work_time += (time_now - mdlstatc->work_start_time);
                mdlstatc->work_start_time = 0;
            }
			break;
		default:
			break;
	}
    utlzpwl_unlock(&gUtlz->lock);

	return IM_RET_OK;
}

IM_UINT32 utlzlib_get_coefficient(IM_TCHAR *name)
{
	utlz_module_statc_t *umblk;
	IM_UINT32 coeffi = 0;
	IM_UINT32 found = 0;
	IM_INFOMSG((IM_STR("%s(name=%s)"), IM_STR(_IM_FUNC_), name));
	IM_ASSERT(gUtlz != IM_NULL);

	utlzpwl_lock(&gUtlz->lock);
	if(gUtlz->umlst != IM_NULL) {
		umblk = (utlz_module_statc_t *)im_list_begin(gUtlz->umlst);
		while (umblk != IM_NULL)
		{
			if(!utlzpwl_strcmp(umblk->module.name, name))
			{
				found = 1;
				coeffi = umblk->coeffi;
				break;
			}
			umblk = (utlz_module_statc_t *)im_list_next(gUtlz->umlst);
		}
	}
	utlzpwl_unlock(&gUtlz->lock);
	if (found == 0)
	{
		IM_ERRMSG((IM_STR("Can not found module(%s) in list, may be not registered"), name));
	       return 0;
	}	       

	return coeffi;
}



