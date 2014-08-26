/*
 * Copyright � 2009 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Boon Wooi Tay<boon.wooi.tay@intel.com>
 *    Kalaiyappan, Periyakaruppan Kumaran<periyakaruppan.kumaran.kalaiyappan@intel.com>
 *
 */

#include "wrapper_drv_video.h"

#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#define ALIGN(i, n)    (((i) + (n) - 1) & ~((n) - 1))

#define DRIVER_EXTENSION	"_drv_video.so"

void vawr_errorMessage(const char *msg, ...)
{
    va_list args;

    fprintf(stderr, "libva-wrapper error: ");
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
}

void vawr_infoMessage(const char *msg, ...)
{
    va_list args;

    fprintf(stderr, "libva-wrapper: ");
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
}

static inline int
va_getDriverInitName(char *name, int namelen, int major, int minor)
{
    int ret = snprintf(name, namelen, "__vaDriverInit_%d_%d", major, minor);
    return ret > 0 && ret < namelen;
}

static VAStatus vawr_openDriver(VADriverContextP ctx, char *driver_name)
{
    VAStatus vaStatus = VA_STATUS_ERROR_UNKNOWN;

    char *driver_dir = "/usr/lib64/va/drivers/";
    void *handle = NULL;

    char *driver_path = (char *) calloc(1, strlen(driver_dir) +
                                         strlen(driver_name) +
                                         strlen(DRIVER_EXTENSION) + 1 );
    if (!driver_path) {
        vawr_errorMessage("%s L%d Out of memory!n",
                            __FUNCTION__, __LINE__);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    strncpy( driver_path, driver_dir, strlen(driver_dir) );
    strncat( driver_path, driver_name, strlen(driver_name) );
    strncat( driver_path, DRIVER_EXTENSION, strlen(DRIVER_EXTENSION) );

    handle = dlopen( driver_path, RTLD_NOW| RTLD_GLOBAL);
    if (!handle) {
        /* Manage error if the file exists or not we need to know what's going
	 * on */
        vawr_errorMessage("dlopen of %s failed: %s\n", driver_path, dlerror());
    } else {
        VADriverInit init_func = NULL;
        char init_func_s[256];
        int i;

        static const struct {
            int major;
            int minor;
        } compatible_versions[] = {
            { VA_MAJOR_VERSION, VA_MINOR_VERSION },
            { 0, 33 },
            { 0, 32 },
            { -1, }
        };

        for (i = 0; compatible_versions[i].major >= 0; i++) {
            if (va_getDriverInitName(init_func_s, sizeof(init_func_s),
                                     compatible_versions[i].major,
                                     compatible_versions[i].minor)) {
                init_func = (VADriverInit)dlsym(handle, init_func_s);
                if (init_func) {
                    vawr_infoMessage("Found init function %s\n", init_func_s);
                    break;
                }
            }
        }

        if (compatible_versions[i].major < 0) {
            vawr_errorMessage("%s has no function %s\n",
                            driver_path, init_func_s);
        } else {
            if (init_func)
                vaStatus = (*init_func)(ctx);
		if (VA_STATUS_SUCCESS != vaStatus) {
			vawr_errorMessage("%s init failed\n", driver_path);
		} else {
			/* TODO: Store the handle to a private structure */
		}
        }
    }
    free(driver_path);
    driver_path=NULL;
    return vaStatus;
}

VAStatus
vawr_Terminate(VADriverContextP ctx)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaTerminate(ctx));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_QueryConfigProfiles(VADriverContextP ctx,
                         VAProfile *profile_list,       /* out */
                         int *num_profiles)             /* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQueryConfigProfiles(ctx, profile_list, num_profiles));
    RESTORE_VAWRDATA(ctx, vawr);

    /* PSB_DRV has not published VP8 profile in QueryConfigProfiles yet,
     * that means we have to do it here.
     */
    profile_list[*num_profiles++] = VAProfileVP8Version0_3;

	return vaStatus;
}

VAStatus
vawr_QueryConfigEntrypoints(VADriverContextP ctx,
                            VAProfile profile,
                            VAEntrypoint *entrypoint_list,      /* out */
                            int *num_entrypoints)               /* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    /* PSB_DRV currently only supports VLD entry point for VP8 */
    if (profile == VAProfileVP8Version0_3) {
	*entrypoint_list = VAEntrypointVLD;
	*num_entrypoints = 1;
	return VA_STATUS_SUCCESS;
    }

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQueryConfigEntrypoints(ctx, profile, entrypoint_list, num_entrypoints));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_GetConfigAttributes(VADriverContextP ctx,
                         VAProfile profile,
                         VAEntrypoint entrypoint,
                         VAConfigAttrib *attrib_list,  /* in/out */
                         int num_attribs)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    int i;

    /* PSB_DRV currently only supports VAConfigAttribRTFormat attribute type */
    if (profile == VAProfileVP8Version0_3) {
        for (i = 0; i < num_attribs; i++) {
		switch (attrib_list[i].type) {
		case VAConfigAttribRTFormat:
			attrib_list[i].value = VA_RT_FORMAT_YUV420;
			break;
		default:
			attrib_list[i].value = VA_ATTRIB_NOT_SUPPORTED;
			break;
		}
        }

        return VA_STATUS_SUCCESS;
    }

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaGetConfigAttributes(ctx, profile, entrypoint, attrib_list, num_attribs));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_CreateConfig(VADriverContextP ctx,
                  VAProfile profile,
                  VAEntrypoint entrypoint,
                  VAConfigAttrib *attrib_list,
                  int num_attribs,
                  VAConfigID *config_id)		/* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    struct VADriverVTable *psb_vtable = NULL;
    struct VADriverVTable * const vtable = ctx->vtable;
    char *driver_name = "pvr";

    if (profile == VAProfileVP8Version0_3) {
	vawr->profile = VAProfileVP8Version0_3;

	/* This is the first time we learn about the config profile,
	 * and we have to load psb_drv_video here if
	 * a VP8 config is to be created.
	 * Is this a good place to load psb_drv_video? We can
	 * load it during vaInitialize together with i965_drv_video,
	 * but it could slow down init for process that doesn't
	 * require VP8.
	 */
        psb_vtable = calloc(1, sizeof(*psb_vtable));
        if (!psb_vtable)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        ctx->vtable = psb_vtable;
        vaStatus = vawr_openDriver(ctx, driver_name);
        if (VA_STATUS_SUCCESS == vaStatus) {
            /* We have successfully initialized i965 video driver,
             * Let's store i965's private driver data and vtable.
             */
            vawr->drv_data[PSB_DRV] = (void *)ctx->pDriverData;
            vawr->drv_vtable[PSB_DRV] = psb_vtable;

            /* Also restore the va's vtable */
            ctx->vtable = vtable;

            /* TODO: Shall we backup ctx structure? Some members like versions, max_num_profiles
             * will be overwritten but are they still important? Most likely not.
             * Let's not do it for now.
             */
        }

    }

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaCreateConfig(ctx, profile, entrypoint, attrib_list, num_attribs, config_id));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_DestroyConfig(VADriverContextP ctx, VAConfigID config_id)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaDestroyConfig(ctx, config_id));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus vawr_QueryConfigAttributes(VADriverContextP ctx,
                                    VAConfigID config_id,
                                    VAProfile *profile,                 /* out */
                                    VAEntrypoint *entrypoint,           /* out */
                                    VAConfigAttrib *attrib_list,        /* out */
                                    int *num_attribs)                   /* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQueryConfigAttributes(ctx, config_id, profile, entrypoint, attrib_list, num_attribs));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_CreateSurfaces(VADriverContextP ctx,
                    int width,
                    int height,
                    int format,
                    int num_surfaces,
                    VASurfaceID *surfaces)
                    //VASurfaceAttrib    *attrib_list,
                    //unsigned int        num_attribs)      /* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    unsigned int h_stride = 0, v_stride = 0;

    /* We will always call i965's vaCreateSurfaces for VA Surface allocation,
     * then if the config profile is VP8 we will map the surface into TTM
     */
    ctx->pDriverData = vawr->drv_data[0];

    // XXX, assume we have already got correct profile (vaCreateConfig is called before vaCreateSurface)
    if (vawr->profile == VAProfileVP8Version0_3) {
        VASurfaceAttrib surface_attrib[2];
        VASurfaceAttribExternalBuffers buffer_attrib;
        int i=0;

        surface_attrib[i].type = VASurfaceAttribMemoryType;
        surface_attrib[i].flags = VA_SURFACE_ATTRIB_SETTABLE;
        surface_attrib[i].value.type = VAGenericValueTypeInteger;
        surface_attrib[i].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_VA;
        i++;

        /* Calculate stride to meet psb requirement */
        if (512 >= width)
            h_stride = 512;
        else if (1024 >= width)
            h_stride = 1024;
        else if (1280 >= width)
            h_stride = 1280;
        else if (2048 >= width)
            h_stride = 2048;
        else if (4096 >= width)
            h_stride = 4096;
        else
            assert(0);
        v_stride = ALIGN(height, 32);
        memset(&buffer_attrib, 0, sizeof(VASurfaceAttribExternalBuffers));
        buffer_attrib.pixel_format = VA_FOURCC_NV12;
        buffer_attrib.width = width;
        buffer_attrib.height= height;
        buffer_attrib.num_planes = 2;
        buffer_attrib.pitches[0] = h_stride;
        buffer_attrib.pitches[1] = h_stride;
        buffer_attrib.offsets[0] = 0;
        buffer_attrib.offsets[1] = h_stride * v_stride;
        buffer_attrib.flags = 0; // tiling is diabled by default

        surface_attrib[i].type = VASurfaceAttribExternalBufferDescriptor;
        surface_attrib[i].flags = VA_SURFACE_ATTRIB_SETTABLE;
        surface_attrib[i].value.type = VAGenericValueTypePointer;
        surface_attrib[i].value.value.p = &buffer_attrib;
        i++;

        vaStatus = vawr->drv_vtable[0]->vaCreateSurfaces2(ctx, format, width, height, surfaces, num_surfaces, &surface_attrib[0], i);
     } else {
        vaStatus = vawr->drv_vtable[0]->vaCreateSurfaces(ctx, width, height, format, num_surfaces, surfaces);
     }

    RESTORE_VAWRDATA(ctx, vawr);
	return vaStatus;
}

VAStatus
vawr_DestroySurfaces(VADriverContextP ctx,
                     VASurfaceID *surface_list,
                     int num_surfaces)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    vawr_surface_lookup_t *surface, *temp;
    int i;

	if (vawr->profile == VAProfileVP8Version0_3) {
		/* First destroy the PVR surfaces */
		for (i=0; i<num_surfaces; i++) {
			LIST_FOR_EACH_ENTRY_SAFE(surface, temp, &vawr->surfaces, link) {
				if (surface->i965_surface == surface_list[i]) {
					/* Restore the PVR's context for DestroySurfaces purpose */
					ctx->pDriverData = vawr->drv_data[1];
					vaStatus = vawr->drv_vtable[1]->vaDestroySurfaces(ctx, &surface->pvr_surface, 1);
					surface->i965_surface = 0;
					surface->pvr_surface = 0;
					LIST_DEL(&surface->link);
				}
			}

		}
	}

	/* Now destroy the i965 surfaces */
    ctx->pDriverData = vawr->drv_data[0];
	vaStatus = vawr->drv_vtable[0]->vaDestroySurfaces(ctx, surface_list, num_surfaces);
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_CreateContext(VADriverContextP ctx,
                   VAConfigID config_id,
                   int picture_width,
                   int picture_height,
                   int flag,
                   VASurfaceID *render_targets,
                   int num_render_targets,
                   VAContextID *context)                /* out */
{
    VAStatus vaStatus;
    VASurfaceID vawr_render_targets[num_render_targets];
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    int i;

    /* If config profile is VP8, we have to map the render targets into pvr driver's TTM
     * Since config_id is opague to wrapper (at least for now), we can only check vawr->profile
     */
	if (vawr->profile == VAProfileVP8Version0_3) {
		vawr_surface_lookup_t *surface;
		LIST_INIT(&vawr->surfaces);

		for (i=0; i<num_render_targets; i++) {
		    VAImage	image;

			/* Call vaDeriveImage of i965 to create a VAImage of corresponding VASurface */
			ctx->pDriverData = vawr->drv_data[0];

			vaStatus = vawr->drv_vtable[0]->vaDeriveImage(ctx, render_targets[i], &image);
			if (vaStatus == VA_STATUS_SUCCESS) {
			    unsigned long long *user_pointer = NULL;

			    /* VAImage has been created successfully, now call i965's vaMapBuffer to
			     * retrieve the user accessible pointer to the surface
			     */
				vaStatus = vawr->drv_vtable[0]->vaMapBuffer(ctx, image.buf, (void **) &user_pointer);
				vawr_errorMessage("CreateContext: user ptr: %p:, addr: %p\n", user_pointer, &user_pointer);
				if (vaStatus == VA_STATUS_SUCCESS) {
					/* TODO: We have to pass the user pointer to pvr's vaCreateSurfaces2
					 * to map this VASurface into pvr's TTM
					 */
					vaStatus = VA_STATUS_ERROR_OPERATION_FAILED;
					if (vawr->drv_vtable[1]->vaCreateSurfaces2) {
						VASurfaceID	surface_id;
						VASurfaceAttrib attrib_list[2] = {};
						VASurfaceAttribExternalBuffers *buffer_descriptor = NULL;

						buffer_descriptor = calloc(1, sizeof(*buffer_descriptor));
						if (!buffer_descriptor) {
							return VA_STATUS_ERROR_ALLOCATION_FAILED;
						}

						buffer_descriptor->num_buffers = 1;
						buffer_descriptor->width = image.width;
						buffer_descriptor->height = image.height;
						//buffer_descriptor->pixel_format = image.format.fourcc; /* format will be used instead of pixel_format */
						//buffer_descriptor->data_size =
						buffer_descriptor->pitches[0] = image.pitches[0];
						buffer_descriptor->pitches[1] = image.pitches[1];
						buffer_descriptor->pitches[2] = image.pitches[1];
						buffer_descriptor->offsets[0] = image.offsets[0];
						buffer_descriptor->offsets[1] = image.offsets[1];
						buffer_descriptor->offsets[2] = image.offsets[1];
						buffer_descriptor->buffers = (unsigned long long *)&user_pointer;

						ctx->pDriverData = vawr->drv_data[1];

						attrib_list[0].type = VASurfaceAttribExternalBufferDescriptor;
						attrib_list[0].value.value.p = buffer_descriptor;

						attrib_list[1].type = VASurfaceAttribMemoryType;
						attrib_list[1].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;

						vaStatus = vawr->drv_vtable[1]->vaCreateSurfaces2(ctx, VA_RT_FORMAT_YUV420, image.width,
										((image.height + 32 - 1) & ~(32 - 1)), &surface_id, 1, &attrib_list[0], 2);
						if (vaStatus == VA_STATUS_SUCCESS){
							/* Find a way to store the returned surface_id with correct mapping of i965's surface_id,
							 * since all future surface_id communicated between application and wrappre is i965's
							 * while one communicated between wrapper and pvr driver is pvr's.
							 */
							surface = calloc(1, sizeof *surface);
							if (!surface) {
								return VA_STATUS_ERROR_ALLOCATION_FAILED;
							}

							surface->i965_surface = render_targets[i];
							surface->pvr_surface = surface_id;
							LIST_ADD(&surface->link,&vawr->surfaces);

							/* render_targets is pvr's surface_id from here on */
							vawr_render_targets[i] = surface_id;
							/* Free allocated memory */
							free(buffer_descriptor);
						}
					}
				}

				/* Unmap the surface buffer */
				ctx->pDriverData = vawr->drv_data[0];
				vaStatus = vawr->drv_vtable[0]->vaUnmapBuffer(ctx, image.buf);
			}
		}
	}

	RESTORE_DRVDATA(ctx, vawr);
    if (vawr->profile == VAProfileVP8Version0_3) {
        CALL_DRVVTABLE(vawr, vaStatus, vaCreateContext(ctx, config_id, picture_width, picture_height, flag, vawr_render_targets, num_render_targets, context));
    } else {
        CALL_DRVVTABLE(vawr, vaStatus, vaCreateContext(ctx, config_id, picture_width, picture_height, flag, render_targets, num_render_targets, context));
    }

    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_DestroyContext(VADriverContextP ctx, VAContextID context)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaDestroyContext(ctx, context));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_CreateBuffer(VADriverContextP ctx,
                  VAContextID context,          /* in */
                  VABufferType type,            /* in */
                  unsigned int size,            /* in */
                  unsigned int num_elements,    /* in */
                  void *data,                   /* in */
                  VABufferID *buf_id)           /* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaCreateBuffer(ctx, context, type, size, num_elements, data, buf_id));
    RESTORE_VAWRDATA(ctx, vawr);

    /* For now, let's track the VP8's VAPictureParameterBufferType buf_id so that we can overwrite the
     * i965's VASurfaceID embedded in the picture parameter with pvr's. This is a dirty hack until we
     * find a better way to deal with surface_id.
     */
	if (vawr->profile == VAProfileVP8Version0_3) {
		if (type == VAPictureParameterBufferType) {
			vawr->pic_param_buf_id = *buf_id;
		}
	}

	return vaStatus;
}

VAStatus
vawr_BufferSetNumElements(VADriverContextP ctx,
                          VABufferID buf_id,           /* in */
                          unsigned int num_elements)   /* in */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaBufferSetNumElements(ctx, buf_id, num_elements));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_BufferInfo(VADriverContextP ctx,
    VABufferID buf_id,          /* in */
    VABufferType *type,         /* out */
    unsigned int *size,         /* out */
    unsigned int *num_elements) /* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaBufferInfo(ctx, buf_id, type, size, num_elements));
    RESTORE_VAWRDATA(ctx, vawr);

    return vaStatus;
}

VAStatus
vawr_MapBuffer(VADriverContextP ctx,
               VABufferID buf_id,       /* in */
               void **pbuf)             /* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaMapBuffer(ctx, buf_id, pbuf));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_UnmapBuffer(VADriverContextP ctx, VABufferID buf_id)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaUnmapBuffer(ctx, buf_id));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_DestroyBuffer(VADriverContextP ctx, VABufferID buffer_id)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaDestroyBuffer(ctx, buffer_id));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_BeginPicture(VADriverContextP ctx,
                  VAContextID context,
                  VASurfaceID render_target)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    VASurfaceID vawr_render_target;
    vawr_surface_lookup_t *surface_lookup;

    GET_SURFACEID(vawr, surface_lookup, render_target, vawr_render_target);
    //vawr_infoMessage("vawr_BeginPicture: render_target %d\n", render_target);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaBeginPicture(ctx, context, vawr_render_target));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_RenderPicture(VADriverContextP ctx,
                   VAContextID context,
                   VABufferID *buffers,
                   int num_buffers)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    VAPictureParameterBufferVP8 * const pic_param;

    RESTORE_DRVDATA(ctx, vawr);

    /* For now, let's track the VP8's VAPictureParameterBufferType buf_id so that we can overwrite the
     * i965's VASurfaceID embedded in the picture parameter with pvr's. This is a dirty hack until we
     * find a better way to deal with surface_id.
     */
	if (vawr->profile == VAProfileVP8Version0_3) {
	    if (vawr->pic_param_buf_id) {
	        vawr_surface_lookup_t *surface_lookup;

		CALL_DRVVTABLE(vawr, vaStatus, vaMapBuffer(ctx, vawr->pic_param_buf_id, (void **)&pic_param));
		/* Ok we have the picture parameter, let's translate parameters with surface_id */
			LIST_FOR_EACH_ENTRY(surface_lookup, &vawr->surfaces, link) {
					if (surface_lookup->i965_surface == pic_param->last_ref_frame)
						pic_param->last_ref_frame = surface_lookup->pvr_surface;
					if (surface_lookup->i965_surface == pic_param->golden_ref_frame)
						pic_param->golden_ref_frame = surface_lookup->pvr_surface;
					if (surface_lookup->i965_surface == pic_param->alt_ref_frame)
						pic_param->alt_ref_frame = surface_lookup->pvr_surface;
			}
		CALL_DRVVTABLE(vawr, vaStatus, vaUnmapBuffer(ctx, vawr->pic_param_buf_id));
		vawr->pic_param_buf_id = 0;
	    }
	}
    CALL_DRVVTABLE(vawr, vaStatus, vaRenderPicture(ctx, context, buffers, num_buffers));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_EndPicture(VADriverContextP ctx, VAContextID context)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaEndPicture(ctx, context));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_SyncSurface(VADriverContextP ctx,
                 VASurfaceID render_target)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    VASurfaceID vawr_render_target;
    vawr_surface_lookup_t *surface_lookup;

    GET_SURFACEID(vawr, surface_lookup, render_target, vawr_render_target);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaSyncSurface(ctx, vawr_render_target));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_QuerySurfaceStatus(VADriverContextP ctx,
                        VASurfaceID render_target,
                        VASurfaceStatus *status)        /* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    VASurfaceID vawr_render_target;
    vawr_surface_lookup_t *surface_lookup;

    GET_SURFACEID(vawr, surface_lookup, render_target, vawr_render_target);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQuerySurfaceStatus(ctx, vawr_render_target, status));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_PutSurface(VADriverContextP ctx,
                VASurfaceID render_target,
                void *draw, /* X Drawable */
                short srcx,
                short srcy,
                unsigned short srcw,
                unsigned short srch,
                short destx,
                short desty,
                unsigned short destw,
                unsigned short desth,
                VARectangle *cliprects, /* client supplied clip list */
                unsigned int number_cliprects, /* number of clip rects in the clip list */
                unsigned int flags) /* de-interlacing flags */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    //VASurfaceID vawr_render_target;
    //vawr_surface_lookup_t *surface_lookup;

    //GET_SURFACEID(vawr, surface_lookup, render_target, vawr_render_target);
	/* Rendering should always be done via i965 */
	ctx->pDriverData = vawr->drv_data[0];
	vaStatus = vawr->drv_vtable[0]->vaPutSurface(ctx, render_target, draw, srcx, srcy, srcw, srch, destx, desty, destw, desth, cliprects, number_cliprects, flags);
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_QueryImageFormats(VADriverContextP ctx,
                       VAImageFormat *format_list,      /* out */
                       int *num_formats)                /* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQueryImageFormats(ctx, format_list, num_formats));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_CreateImage(VADriverContextP ctx,
                 VAImageFormat *format,
                 int width,
                 int height,
                 VAImage *out_image)        /* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaCreateImage(ctx, format, width, height, out_image));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus vawr_DeriveImage(VADriverContextP ctx,
                          VASurfaceID surface,
                          VAImage *out_image)        /* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    VASurfaceID vawr_surface;
    vawr_surface_lookup_t *surface_lookup;

    GET_SURFACEID(vawr, surface_lookup, surface, vawr_surface);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaDeriveImage(ctx, vawr_surface, out_image));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_DestroyImage(VADriverContextP ctx, VAImageID image)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaDestroyImage(ctx, image));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_SetImagePalette(VADriverContextP ctx,
                     VAImageID image,
                     unsigned char *palette)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaSetImagePalette(ctx, image, palette));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_GetImage(VADriverContextP ctx,
              VASurfaceID surface,
              int x,   /* coordinates of the upper left source pixel */
              int y,
              unsigned int width,      /* width and height of the region */
              unsigned int height,
              VAImageID image)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    VASurfaceID vawr_surface;
    vawr_surface_lookup_t *surface_lookup;

    GET_SURFACEID(vawr, surface_lookup, surface, vawr_surface);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaGetImage(ctx, vawr_surface, x, y, width, height, image));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_PutImage(VADriverContextP ctx,
              VASurfaceID surface,
              VAImageID image,
              int src_x,
              int src_y,
              unsigned int src_width,
              unsigned int src_height,
              int dest_x,
              int dest_y,
              unsigned int dest_width,
              unsigned int dest_height)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    VASurfaceID vawr_surface;
    vawr_surface_lookup_t *surface_lookup;

    GET_SURFACEID(vawr, surface_lookup, surface, vawr_surface);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaPutImage(ctx, vawr_surface, image, src_x, src_y, src_width, src_height, dest_x, dest_y, dest_width, dest_height));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_QuerySubpictureFormats(VADriverContextP ctx,
                            VAImageFormat *format_list,         /* out */
                            unsigned int *flags,                /* out */
                            unsigned int *num_formats)          /* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQuerySubpictureFormats(ctx, format_list, flags, num_formats));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_CreateSubpicture(VADriverContextP ctx,
                      VAImageID image,
                      VASubpictureID *subpicture)         /* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaCreateSubpicture(ctx, image, subpicture));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_DestroySubpicture(VADriverContextP ctx,
                       VASubpictureID subpicture)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaDestroySubpicture(ctx, subpicture));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_SetSubpictureImage(VADriverContextP ctx,
                        VASubpictureID subpicture,
                        VAImageID image)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaSetSubpictureImage(ctx, subpicture, image));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_SetSubpictureChromakey(VADriverContextP ctx,
                            VASubpictureID subpicture,
                            unsigned int chromakey_min,
                            unsigned int chromakey_max,
                            unsigned int chromakey_mask)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaSetSubpictureChromakey(ctx, subpicture, chromakey_min, chromakey_max, chromakey_mask));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_SetSubpictureGlobalAlpha(VADriverContextP ctx,
                              VASubpictureID subpicture,
                              float global_alpha)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaSetSubpictureGlobalAlpha(ctx, subpicture, global_alpha));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_AssociateSubpicture(VADriverContextP ctx,
                         VASubpictureID subpicture,
                         VASurfaceID *target_surfaces,
                         int num_surfaces,
                         short src_x, /* upper left offset in subpicture */
                         short src_y,
                         unsigned short src_width,
                         unsigned short src_height,
                         short dest_x, /* upper left offset in surface */
                         short dest_y,
                         unsigned short dest_width,
                         unsigned short dest_height,
                         /*
                          * whether to enable chroma-keying or global-alpha
                          * see VA_SUBPICTURE_XXX values
                          */
                         unsigned int flags)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaAssociateSubpicture(ctx, subpicture, target_surfaces, num_surfaces, src_x, src_y, src_width, src_height, dest_x, dest_y, dest_width, dest_height, flags));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_DeassociateSubpicture(VADriverContextP ctx,
                           VASubpictureID subpicture,
                           VASurfaceID *target_surfaces,
                           int num_surfaces)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaDeassociateSubpicture(ctx, subpicture, target_surfaces, num_surfaces));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_QueryDisplayAttributes(VADriverContextP ctx,
                            VADisplayAttribute *attr_list,	/* out */
                            int *num_attributes)		/* out */
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQueryDisplayAttributes(ctx, attr_list, num_attributes));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;

}

VAStatus
vawr_GetDisplayAttributes(VADriverContextP ctx,
                          VADisplayAttribute *attr_list,	/* in/out */
                          int num_attributes)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaGetDisplayAttributes(ctx, attr_list, num_attributes));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;

}

VAStatus
vawr_SetDisplayAttributes(VADriverContextP ctx,
                          VADisplayAttribute *attr_list,
                          int num_attributes)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaSetDisplayAttributes(ctx, attr_list, num_attributes));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;

}

VAStatus vawr_LockSurface(VADriverContextP ctx,
    VASurfaceID surface,
    unsigned int *fourcc, /* following are output argument */
    unsigned int *luma_stride,
    unsigned int *chroma_u_stride,
    unsigned int *chroma_v_stride,
    unsigned int *luma_offset,
    unsigned int *chroma_u_offset,
    unsigned int *chroma_v_offset,
    unsigned int *buffer_name,
    void **buffer)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    VASurfaceID vawr_render_target;
    vawr_surface_lookup_t *surface_lookup;

    GET_SURFACEID(vawr, surface_lookup, surface, vawr_render_target);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaLockSurface(ctx, vawr_render_target, fourcc, luma_stride, chroma_u_stride, chroma_v_stride, luma_offset, chroma_u_offset, chroma_v_offset, buffer_name, buffer));
    RESTORE_VAWRDATA(ctx, vawr);

    return vaStatus;
}


VAStatus vawr_UnlockSurface(VADriverContextP ctx,
    VASurfaceID surface)
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr = GET_VAWRDATA(ctx);
    VASurfaceID vawr_render_target;
    vawr_surface_lookup_t *surface_lookup;

    GET_SURFACEID(vawr, surface_lookup, surface, vawr_render_target);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaUnlockSurface( ctx, vawr_render_target));
    RESTORE_VAWRDATA(ctx, vawr);

    return vaStatus;
}

VAStatus DLL_EXPORT
__vaDriverInit_0_32(VADriverContextP ctx);

VAStatus
__vaDriverInit_0_32(  VADriverContextP ctx )
{
    VAStatus vaStatus;
    struct vawr_driver_data *vawr;
    struct VADriverVTable *i965_vtable = NULL;
    struct VADriverVTable * const vtable = ctx->vtable;
    char *driver_name = "i965";

    vawr = calloc(1, sizeof(*vawr));
    if (!vawr)
	return VA_STATUS_ERROR_ALLOCATION_FAILED;

    i965_vtable = calloc(1, sizeof(*i965_vtable));
    if (!i965_vtable)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    /* Store i965_ctx into wrapper's private driver data
     * It will later be used when i965 VA API is called
     */

    /* By default we should load and initialize OTC's i965 video driver */

    /* First, hook i965_vtable to ctx so that i965_drv_video uses it. */
    ctx->vtable = i965_vtable;

    /* Then, load the i965 driver */
    vaStatus = vawr_openDriver(ctx, driver_name);
    if (VA_STATUS_SUCCESS == vaStatus) {
	/* We have successfully initialized i965 video driver,
	 * Let's store i965's private driver data and vtable.
	 */
        vawr->drv_data[I965_DRV] = (void *)ctx->pDriverData;
        vawr->drv_vtable[I965_DRV] = i965_vtable;
        vawr->profile = VAProfileNone;

        /* Also restore the va's vtable */
        ctx->vtable = vtable;

        /* Increase max num of profiles and entrypoints for PSB's VP8 profile */
        ctx->max_profiles = ctx->max_profiles + 1;
        ctx->max_entrypoints = ctx->max_entrypoints + 1;

        /* Populate va wrapper's vtable */
        vtable->vaTerminate = vawr_Terminate;
        vtable->vaQueryConfigEntrypoints = vawr_QueryConfigEntrypoints;
        vtable->vaQueryConfigProfiles = vawr_QueryConfigProfiles;
        vtable->vaQueryConfigEntrypoints = vawr_QueryConfigEntrypoints;
        vtable->vaQueryConfigAttributes = vawr_QueryConfigAttributes;
        vtable->vaCreateConfig = vawr_CreateConfig;
        vtable->vaDestroyConfig = vawr_DestroyConfig;
        vtable->vaGetConfigAttributes = vawr_GetConfigAttributes;
        vtable->vaCreateSurfaces = vawr_CreateSurfaces;
        vtable->vaDestroySurfaces = vawr_DestroySurfaces;
        vtable->vaCreateContext = vawr_CreateContext;
        vtable->vaDestroyContext = vawr_DestroyContext;
        vtable->vaCreateBuffer = vawr_CreateBuffer;
        vtable->vaBufferSetNumElements = vawr_BufferSetNumElements;
        vtable->vaBufferInfo = vawr_BufferInfo;
        vtable->vaMapBuffer = vawr_MapBuffer;
        vtable->vaUnmapBuffer = vawr_UnmapBuffer;
        vtable->vaDestroyBuffer = vawr_DestroyBuffer;
        vtable->vaBeginPicture = vawr_BeginPicture;
        vtable->vaRenderPicture = vawr_RenderPicture;
        vtable->vaEndPicture = vawr_EndPicture;
        vtable->vaSyncSurface = vawr_SyncSurface;
        vtable->vaQuerySurfaceStatus = vawr_QuerySurfaceStatus;
        vtable->vaPutSurface = vawr_PutSurface;
        vtable->vaQueryImageFormats = vawr_QueryImageFormats;
        vtable->vaCreateImage = vawr_CreateImage;
        vtable->vaDeriveImage = vawr_DeriveImage;
        vtable->vaDestroyImage = vawr_DestroyImage;
        vtable->vaSetImagePalette = vawr_SetImagePalette;
        vtable->vaGetImage = vawr_GetImage;
        vtable->vaPutImage = vawr_PutImage;
        vtable->vaQuerySubpictureFormats = vawr_QuerySubpictureFormats;
        vtable->vaCreateSubpicture = vawr_CreateSubpicture;
        vtable->vaDestroySubpicture = vawr_DestroySubpicture;
        vtable->vaSetSubpictureImage = vawr_SetSubpictureImage;
        vtable->vaSetSubpictureChromakey = vawr_SetSubpictureChromakey;
        vtable->vaSetSubpictureGlobalAlpha = vawr_SetSubpictureGlobalAlpha;
        vtable->vaAssociateSubpicture = vawr_AssociateSubpicture;
        vtable->vaDeassociateSubpicture = vawr_DeassociateSubpicture;
        vtable->vaQueryDisplayAttributes = vawr_QueryDisplayAttributes;
        vtable->vaGetDisplayAttributes = vawr_GetDisplayAttributes;
        vtable->vaSetDisplayAttributes = vawr_SetDisplayAttributes;
        vtable->vaLockSurface = vawr_LockSurface;
        vtable->vaUnlockSurface= vawr_UnlockSurface;
    }

    /* Store wrapper's private driver data*/
    ctx->pDriverData = (void *)vawr;

    return vaStatus;
}
