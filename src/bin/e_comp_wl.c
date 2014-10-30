#define E_COMP_WL
#include "e.h"

#define COMPOSITOR_VERSION 3

#define E_COMP_WL_PIXMAP_CHECK \
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return

/* Resource Data Mapping: (wl_resource_get_user_data)
 * 
 * wl_surface == e_pixmap
 * wl_region == eina_tiler
 * 
 */

/* local variables */
static Eina_Hash *clients_win_hash = NULL;

/* local functions */
static void 
_e_comp_wl_log_cb_print(const char *format, va_list args)
{
   EINA_LOG_DOM_INFO(e_log_dom, format, args);
}

static Eina_Bool 
_e_comp_wl_cb_read(void *data, Ecore_Fd_Handler *hdlr EINA_UNUSED)
{
   E_Comp_Data *cdata;

   if (!(cdata = data)) return ECORE_CALLBACK_RENEW;

   /* dispatch pending wayland events */
   wl_event_loop_dispatch(cdata->wl.loop, 0);

   return ECORE_CALLBACK_RENEW;
}

static void 
_e_comp_wl_cb_prepare(void *data, Ecore_Fd_Handler *hdlr EINA_UNUSED)
{
   E_Comp_Data *cdata;

   if (!(cdata = data)) return;

   /* flush pending client events */
   wl_display_flush_clients(cdata->wl.disp);
}

static Eina_Bool 
_e_comp_wl_cb_module_idle(void *data)
{
   E_Comp_Data *cdata;
   E_Module  *mod = NULL;

   if (!(cdata = data)) return ECORE_CALLBACK_RENEW;

   /* check if we are still loading modules */
   if (e_module_loading_get()) return ECORE_CALLBACK_RENEW;

   if (!(mod = e_module_find("wl_desktop_shell")))
     mod = e_module_new("wl_desktop_shell");

   if (mod)
     {
        e_module_enable(mod);

        /* FIXME: NB: 
         * Do we need to dispatch pending wl events here ?? */

        return ECORE_CALLBACK_CANCEL;
     }

   return ECORE_CALLBACK_RENEW;
}

static void 
_e_comp_wl_buffer_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Comp_Wl_Buffer *buffer;

   DBG("Buffer Cb Destroy");

   /* try to get the buffer from the listener */
   if ((buffer = container_of(listener, E_Comp_Wl_Buffer, destroy_listener)))
     {
        DBG("\tEmit buffer destroy signal");
        /* emit the destroy signal */
        wl_signal_emit(&buffer->destroy_signal, buffer);

        /* FIXME: Investigate validity of this
         * 
         * I think this could be a problem because the destroy signal 
         * uses the buffer as the 'data', so anything that catches 
         * this signal is going to run into problems if we free */
        free(buffer);
     }
}

static E_Comp_Wl_Buffer *
_e_comp_wl_buffer_get(struct wl_resource *resource)
{
   E_Comp_Wl_Buffer *buffer;
   struct wl_listener *listener;

   /* try to get the destroy listener from this resource */
   listener = 
     wl_resource_get_destroy_listener(resource, _e_comp_wl_buffer_cb_destroy);

   /* if we have the destroy listener, return the E_Comp_Wl_Buffer */
   if (listener)
     return container_of(listener, E_Comp_Wl_Buffer, destroy_listener);

   /* no destroy listener on this resource, try to create new buffer */
   if (!(buffer = E_NEW(E_Comp_Wl_Buffer, 1))) return NULL;

   /* initialize buffer structure */
   buffer->resource = resource;
   wl_signal_init(&buffer->destroy_signal);

   /* setup buffer destroy callback */
   buffer->destroy_listener.notify = _e_comp_wl_buffer_cb_destroy;
   wl_resource_add_destroy_listener(resource, &buffer->destroy_listener);

   return buffer;
}

static void 
_e_comp_wl_surface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Pixmap *ep;

   DBG("Surface Cb Destroy: %d", wl_resource_get_id(resource));

   /* unset the pixmap resource */
   if ((ep = wl_resource_get_user_data(resource)))
     e_pixmap_resource_set(ep, NULL);

   /* destroy this resource */
   wl_resource_destroy(resource);
}

static void 
_e_comp_wl_surface_cb_attach(struct wl_client *client, struct wl_resource *resource, struct wl_resource *buffer_resource, int32_t sx, int32_t sy)
{
   E_Pixmap *ep;
   uint64_t pixid;
   E_Client *ec;

   DBG("Surface Attach: %d", wl_resource_get_id(resource));

   /* get the e_pixmap reference */
   if (!(ep = wl_resource_get_user_data(resource))) return;

   pixid = e_pixmap_window_get(ep);
   DBG("\tSurface has Pixmap: %llu", pixid);

   /* try to find the associated e_client */
   if (!(ec = e_pixmap_client_get(ep)))
     {
        if (!(ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, pixid)))
          {
             ERR("\tCould not find client from pixmap %llu", pixid);
             return;
          }
     }

   /* check if client is being deleted */
   if (e_object_is_del(E_OBJECT(ec)))
     {
        DBG("\tE_Client scheduled for deletion");
        return;
     }

   /* check for valid comp_data */
   if (!ec->comp_data)
     {
        ERR("\tE_Client has no comp data");
        return;
     }

   /* clear any pending buffer
    * 
    * NB: This will call any buffer_destroy function associated */
   if (ec->comp_data->pending.buffer)
     wl_list_remove(&ec->comp_data->pending.buffer_destroy.link);

   /* reset client pending information */
   ec->comp_data->pending.x = 0;
   ec->comp_data->pending.y = 0;
   ec->comp_data->pending.w = 0;
   ec->comp_data->pending.h = 0;
   ec->comp_data->pending.buffer = NULL;
   ec->comp_data->pending.new_attach = EINA_TRUE;

   if (buffer_resource)
     {
        E_Comp_Wl_Buffer *buffer = NULL;
        struct wl_shm_buffer *shmb;

        /* try to get the E_Comp_Wl_Buffer */
        if (!(buffer = _e_comp_wl_buffer_get(buffer_resource)))
          {
             ERR("\tCould not get E_Comp_Wl_Buffer");
             wl_client_post_no_memory(client);
             return;
          }

        /* since we have a valid buffer, set pending properties */
        ec->comp_data->pending.x = sx;
        ec->comp_data->pending.y = sy;
        ec->comp_data->pending.buffer = buffer;

        /* check for this resource being a shm buffer */
        if ((shmb = wl_shm_buffer_get(buffer_resource)))
          {
             /* update pending size */
             ec->comp_data->pending.w = wl_shm_buffer_get_width(shmb);
             ec->comp_data->pending.h = wl_shm_buffer_get_height(shmb);
          }

        /* add buffer destroy signal so we get notified when this buffer 
         * gets destroyed (callback set in buffer_get function) */
        wl_signal_add(&buffer->destroy_signal, 
                      &ec->comp_data->pending.buffer_destroy);
     }
}

static void 
_e_comp_wl_surface_cb_damage(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y, int32_t w, int32_t h)
{
   E_Pixmap *ep;
   uint64_t pixid;
   E_Client *ec;
   Eina_Rectangle *dmg = NULL;
   int pw, ph;

   DBG("Surface Cb Damage: %d", wl_resource_get_id(resource));
   DBG("\tGeom: %d %d %d %d", x, y, w, h);

   /* get the e_pixmap reference */
   if (!(ep = wl_resource_get_user_data(resource))) return;

   pixid = e_pixmap_window_get(ep);
   DBG("\tSurface has Pixmap: %llu", pixid);

   /* try to find the associated e_client */
   if (!(ec = e_pixmap_client_get(ep)))
     {
        if (!(ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, pixid)))
          {
             ERR("\tCould not find client from pixmap %llu", pixid);
             return;
          }
     }

   if (!ec->comp_data) return;

   e_pixmap_size_get(ep, &pw, &ph);
   DBG("\tPixmap Size: %d %d", pw, ph);

   DBG("\tPending Size: %d %d", ec->comp_data->pending.w, 
       ec->comp_data->pending.h);

   DBG("\tE Client Size: %d %d", ec->client.w, ec->client.h);
   DBG("\tE Size: %d %d", ec->w, ec->h);

   /* create new damage rectangle */
   dmg = eina_rectangle_new(x, y, w, h);

   /* add damage rectangle to list of pending damages */
   ec->comp_data->pending.damages = 
     eina_list_append(ec->comp_data->pending.damages, dmg);
}

static void 
_e_comp_wl_surface_cb_frame(struct wl_client *client, struct wl_resource *resource, uint32_t callback)
{
   DBG("Surface Cb Frame: %d", wl_resource_get_id(resource));
}

static void 
_e_comp_wl_surface_cb_opaque_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource)
{
   E_Pixmap *ep;
   uint64_t pixid;
   E_Client *ec;

   DBG("Surface Opaque Region Set: %d", wl_resource_get_id(resource));

   /* get the e_pixmap reference */
   if (!(ep = wl_resource_get_user_data(resource))) return;

   /* try to find the associated e_client */
   if (!(ec = e_pixmap_client_get(ep)))
     {
        if (!(ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, pixid)))
          {
             ERR("\tCould not find client from pixmap %llu", pixid);
             return;
          }
     }

   /* trap for clients which are being deleted */
   if (e_object_is_del(E_OBJECT(ec))) return;

   if (region_resource)
     {
        Eina_Tiler *tmp;

        /* try to get the tiler from the region resource */
        if (!(tmp = wl_resource_get_user_data(region_resource)))
          return;

        eina_tiler_union(ec->comp_data->pending.opaque, tmp);
     }
   else
     {
        eina_tiler_clear(ec->comp_data->pending.opaque);
        eina_tiler_rect_add(ec->comp_data->pending.opaque, 
                            &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});
     }
}

static void 
_e_comp_wl_surface_cb_input_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource)
{
   E_Pixmap *ep;
   uint64_t pixid;
   E_Client *ec;

   DBG("Surface Input Region Set: %d", wl_resource_get_id(resource));

   /* get the e_pixmap reference */
   if (!(ep = wl_resource_get_user_data(resource))) return;

   /* try to find the associated e_client */
   if (!(ec = e_pixmap_client_get(ep)))
     {
        if (!(ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, pixid)))
          {
             ERR("\tCould not find client from pixmap %llu", pixid);
             return;
          }
     }

   /* trap for clients which are being deleted */
   if (e_object_is_del(E_OBJECT(ec))) return;

   if (region_resource)
     {
        Eina_Tiler *tmp;

        /* try to get the tiler from the region resource */
        if (!(tmp = wl_resource_get_user_data(region_resource)))
          return;

        eina_tiler_union(ec->comp_data->pending.input, tmp);
     }
   else
     {
        eina_tiler_clear(ec->comp_data->pending.input);
        eina_tiler_rect_add(ec->comp_data->pending.input, 
                            &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});
     }
}

static void 
_e_comp_wl_surface_cb_commit(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Pixmap *ep;
   uint64_t pixid;
   E_Client *ec;

   DBG("Surface Commit: %d", wl_resource_get_id(resource));

   /* get the e_pixmap reference */
   if (!(ep = wl_resource_get_user_data(resource))) return;

   pixid = e_pixmap_window_get(ep);
   DBG("\tSurface has Pixmap: %llu", pixid);

   /* try to find the associated e_client */
   if (!(ec = e_pixmap_client_get(ep)))
     {
        if (!(ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, pixid)))
          {
             ERR("\tCould not find client from pixmap %llu", pixid);
             return;
          }
     }

   /* trap for clients which are being deleted */
   if (e_object_is_del(E_OBJECT(ec))) return;

   /* handle actual surface commit */
   if (!e_comp_wl_surface_commit(ec))
     ERR("Failed to commit surface: %d", wl_resource_get_id(resource));
}

static void 
_e_comp_wl_surface_cb_buffer_transform_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int32_t transform EINA_UNUSED)
{
   DBG("Surface Buffer Transform: %d", wl_resource_get_id(resource));
}

static void 
_e_comp_wl_surface_cb_buffer_scale_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int32_t scale EINA_UNUSED)
{
   DBG("Surface Buffer Scale: %d", wl_resource_get_id(resource));
}

static const struct wl_surface_interface _e_surface_interface = 
{
   _e_comp_wl_surface_cb_destroy,
   _e_comp_wl_surface_cb_attach,
   _e_comp_wl_surface_cb_damage,
   _e_comp_wl_surface_cb_frame,
   _e_comp_wl_surface_cb_opaque_region_set,
   _e_comp_wl_surface_cb_input_region_set,
   _e_comp_wl_surface_cb_commit,
   _e_comp_wl_surface_cb_buffer_transform_set,
   _e_comp_wl_surface_cb_buffer_scale_set
};

static void 
_e_comp_wl_compositor_cb_surface_create(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   E_Comp *comp;
   struct wl_resource *res;
   uint64_t wid;
   pid_t pid;
   E_Pixmap *ep;

   if (!(comp = wl_resource_get_user_data(resource))) return;

   DBG("Compositor Cb Surface Create: %d", id);

   /* try to create an internal surface */
   if (!(res = wl_resource_create(client, &wl_surface_interface, 
                                  wl_resource_get_version(resource), id)))
     {
        ERR("Could not create compositor surface");
        wl_client_post_no_memory(client);
        return;
     }

   DBG("\tCreated Resource: %d", wl_resource_get_id(res));

   /* FIXME: set callback ? */
   /* set implementation on resource */
   wl_resource_set_implementation(res, &_e_surface_interface, NULL, NULL);
//                                  _callback);

   /* get the client pid and generate a pixmap id */
   wl_client_get_credentials(client, &pid, NULL, NULL);
   wid = e_comp_wl_id_get(pid, id);

   DBG("\tClient Pid: %d", pid);

   /* check for existing pixmap */
   if (!(ep = e_pixmap_find(E_PIXMAP_TYPE_WL, wid)))
     {
        /* try to create new pixmap */
        if (!(ep = e_pixmap_new(E_PIXMAP_TYPE_WL, wid)))
          {
             ERR("Could not create new pixmap");
             wl_resource_destroy(res);
             wl_client_post_no_memory(client);
             return;
          }
     }

   DBG("\tUsing Pixmap: %llu", wid);

   /* set reference to pixmap so we can fetch it later */
   wl_resource_set_user_data(res, ep);

   /* emit surface create signal */
   wl_signal_emit(&comp->wl_comp_data->signals.surface.create, res);
}

static void 
_e_comp_wl_region_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   DBG("Region Destroy: %d", wl_resource_get_id(resource));
   wl_resource_destroy(resource);
}

static void 
_e_comp_wl_region_cb_add(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y, int32_t w, int32_t h)
{
   Eina_Tiler *tiler;

   DBG("Region Add: %d", wl_resource_get_id(resource));
   DBG("\tGeom: %d %d %d %d", x, y, w, h);

   /* get the tiler from the resource */
   if ((tiler = wl_resource_get_user_data(resource)))
     {
        Eina_Tiler *src;

        src = eina_tiler_new(w, h);
        eina_tiler_tile_size_set(src, 1, 1);
        eina_tiler_rect_add(src, &(Eina_Rectangle){x, y, w, h});
        eina_tiler_union(tiler, src);
        eina_tiler_free(src);
     }
}

static void 
_e_comp_wl_region_cb_subtract(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y, int32_t w, int32_t h)
{
   Eina_Tiler *tiler;

   DBG("Region Subtract: %d", wl_resource_get_id(resource));
   DBG("\tGeom: %d %d %d %d", x, y, w, h);

   /* get the tiler from the resource */
   if ((tiler = wl_resource_get_user_data(resource)))
     {
        Eina_Tiler *src;

        src = eina_tiler_new(w, h);
        eina_tiler_tile_size_set(src, 1, 1);
        eina_tiler_rect_add(src, &(Eina_Rectangle){x, y, w, h});

        eina_tiler_subtract(tiler, src);
        eina_tiler_free(src);
     }
}

static const struct wl_region_interface _e_region_interface = 
{
   _e_comp_wl_region_cb_destroy, 
   _e_comp_wl_region_cb_add, 
   _e_comp_wl_region_cb_subtract
};

static void 
_e_comp_wl_compositor_cb_region_destroy(struct wl_resource *resource)
{
   Eina_Tiler *tiler;

   DBG("Compositor Region Destroy: %d", wl_resource_get_id(resource));

   /* try to get the tiler from the region resource */
   if ((tiler = wl_resource_get_user_data(resource)))
     eina_tiler_free(tiler);
}

static void 
_e_comp_wl_compositor_cb_region_create(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   E_Comp *comp;
   Eina_Tiler *tiler;
   struct wl_resource *res;

   /* get the compositor from the resource */
   if (!(comp = wl_resource_get_user_data(resource))) return;

   DBG("Region Create: %d", wl_resource_get_id(resource));

   /* try to create new tiler */
   if (!(tiler = eina_tiler_new(comp->man->w, comp->man->h)))
     {
        ERR("Could not create Eina_Tiler");
        wl_resource_post_no_memory(resource);
        return;
     }

   /* set tiler size */
   eina_tiler_tile_size_set(tiler, 1, 1);

   /* add rectangle to tiler */
   eina_tiler_rect_add(tiler, 
                       &(Eina_Rectangle){0, 0, comp->man->w, comp->man->h});

   if (!(res = wl_resource_create(client, &wl_region_interface, 1, id)))
     {
        ERR("\tFailed to create region resource");
        wl_resource_post_no_memory(resource);
        return;
     }

   wl_resource_set_implementation(res, &_e_region_interface, tiler, 
                                  _e_comp_wl_compositor_cb_region_destroy);
}

static const struct wl_compositor_interface _e_comp_interface = 
{
   _e_comp_wl_compositor_cb_surface_create,
   _e_comp_wl_compositor_cb_region_create
};

static void 
_e_comp_wl_compositor_cb_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_Comp *comp;
   struct wl_resource *res;

   if (!(comp = data)) return;

   if (!(res = 
         wl_resource_create(client, &wl_compositor_interface, 
                            MIN(version, COMPOSITOR_VERSION), id)))
     {
        ERR("Could not create compositor resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_comp_interface, comp, NULL);
}

static void 
_e_comp_wl_compositor_cb_del(E_Comp *comp)
{
   E_Comp_Data *cdata;

   /* get existing compositor data */
   if (!(cdata = comp->wl_comp_data)) return;

   /* delete fd handler */
   if (cdata->fd_hdlr) ecore_main_fd_handler_del(cdata->fd_hdlr);

   /* free allocated data structure */
   free(cdata);
}

static void 
_e_comp_wl_client_cb_new(void *data EINA_UNUSED, E_Client *ec)
{
   uint64_t win;

   DBG("Comp Hook Client New");

   /* make sure this is a wayland client */
   E_COMP_WL_PIXMAP_CHECK;

   /* get window id from pixmap */
   win = e_pixmap_window_get(ec->pixmap);

   /* ignore fake root windows */
   if ((ec->override) && ((ec->x == -77) && (ec->y == -77)))
     {
        e_comp_ignore_win_add(E_PIXMAP_TYPE_WL, win);
        e_object_del(E_OBJECT(ec));
        return;
     }

   if (!(ec->comp_data = E_NEW(E_Comp_Client_Data, 1)))
     {
        ERR("Could not allocate new client data structure");
        return;
     }

   /* create client tilers */
   ec->comp_data->pending.input = eina_tiler_new(ec->w, ec->h);
   eina_tiler_tile_size_set(ec->comp_data->pending.input, 1, 1);

   ec->comp_data->pending.opaque = eina_tiler_new(ec->w, ec->h);
   eina_tiler_tile_size_set(ec->comp_data->pending.opaque, 1, 1);

   /* set initial client properties */
   ec->ignored = e_comp_ignore_win_find(win);
   ec->border_size = 0;
   ec->placed |= ec->override;
   ec->new_client ^= ec->override;
   ec->icccm.accepts_focus = ((!ec->override) && (!ec->input_only));

   /* NB: could not find a better place to do this, BUT for internal windows, 
    * we need to set delete_request else the close buttons on the frames do 
    * basically nothing */
   if (ec->internal) ec->icccm.delete_request = EINA_TRUE;

   /* set initial client data properties */
   ec->comp_data->mapped = EINA_FALSE;
   ec->comp_data->first_damage = ((ec->internal) || (ec->override));

   if ((!e_client_util_ignored_get(ec)) && 
       (!ec->internal) && (!ec->internal_ecore_evas))
     {
        ec->comp_data->need_reparent = EINA_TRUE;
        ec->take_focus = !starting;
     }

   /* add this client to the hash */
   eina_hash_add(clients_win_hash, &win, ec);
   e_hints_client_list_set();

   /* TODO: first draw timer ? */
}

static void 
_e_comp_wl_client_cb_del(void *data EINA_UNUSED, E_Client *ec)
{
   uint64_t win;
   Eina_Rectangle *dmg;

   DBG("Comp Hook Client Del");

   /* make sure this is a wayland client */
   E_COMP_WL_PIXMAP_CHECK;

   /* get window id from pixmap */
   win = e_pixmap_window_get(ec->pixmap);
   eina_hash_del_by_key(clients_win_hash, &win);

   /* TODO: Focus set down */

   ec->already_unparented = EINA_TRUE;
   if (ec->comp_data->reparented)
     {
        /* get the parent window */
        win = e_client_util_pwin_get(ec);

        /* remove the parent from the hash */
        eina_hash_del_by_key(clients_win_hash, &win);

        /* reset pixmap parent window */
        e_pixmap_parent_window_set(ec->pixmap, 0);
     }

   if ((ec->parent) && (ec->parent->modal == ec))
     {
        ec->parent->lock_close = EINA_FALSE;
        ec->parent->modal = NULL;
     }

   if (ec->comp_data->pending.opaque)
     eina_tiler_free(ec->comp_data->pending.opaque);

   EINA_LIST_FREE(ec->comp_data->pending.damage, dmg)
     eina_rectangle_free(dmg);

   if (ec->comp_data->pending.input)
     eina_tiler_free(ec->comp_data->pending.input);

   E_FREE(ec->comp_data);

   /* TODO: focus check */
}

static Eina_Bool 
_e_comp_wl_compositor_create(void)
{
   E_Comp *comp;
   E_Comp_Data *cdata;
   const char *name;
   int fd = 0;

   /* check for existing compositor. create if needed */
   if (!(comp = e_comp_get(NULL)))
     {
        comp = e_comp_new();
        comp->comp_type = E_PIXMAP_TYPE_WL;
        E_OBJECT_DEL_SET(comp, _e_comp_wl_compositor_cb_del);
     }

   /* create new compositor data */
   cdata = E_NEW(E_Comp_Data, 1);

   /* set wayland log handler */
   wl_log_set_handler_server(_e_comp_wl_log_cb_print);

   /* try to create a wayland display */
   if (!(cdata->wl.disp = wl_display_create()))
     {
        ERR("Could not create a Wayland display: %m");
        goto disp_err;
     }

   /* try to setup wayland socket */
   if (!(name = wl_display_add_socket_auto(cdata->wl.disp)))
     {
        ERR("Could not create Wayland display socket: %m");
        goto sock_err;
     }

   /* set wayland display environment variable */
   e_env_set("WAYLAND_DISPLAY", name);

   /* initialize compositor signals */
   wl_signal_init(&cdata->signals.surface.create);
   wl_signal_init(&cdata->signals.surface.activate);
   wl_signal_init(&cdata->signals.surface.kill);

   /* try to add compositor to wayland globals */
   if (!wl_global_create(cdata->wl.disp, &wl_compositor_interface, 
                         COMPOSITOR_VERSION, comp, 
                         _e_comp_wl_compositor_cb_bind))
     {
        ERR("Could not add compositor to wayland globals: %m");
        goto comp_global_err;
     }

   /* try to init data manager */
   if (!e_comp_wl_data_manager_init(cdata))
     {
        ERR("Could not initialize data manager");
        goto data_err;
     }

   /* try to init input */
   if (!e_comp_wl_input_init(cdata))
     {
        ERR("Could not initialize input");
        goto input_err;
     }

   /* initialize shm mechanism */
   wl_display_init_shm(cdata->wl.disp);

   /* get the wayland display loop */
   cdata->wl.loop = wl_display_get_event_loop(cdata->wl.disp);

   /* get the file descriptor of the wayland event loop */
   fd = wl_event_loop_get_fd(cdata->wl.loop);

   /* create a listener for wayland main loop events */
   cdata->fd_hdlr = 
     ecore_main_fd_handler_add(fd, (ECORE_FD_READ | ECORE_FD_ERROR), 
                               _e_comp_wl_cb_read, cdata, NULL, NULL);
   ecore_main_fd_handler_prepare_callback_set(cdata->fd_hdlr, 
                                              _e_comp_wl_cb_prepare, cdata);

   /* setup module idler to load shell mmodule */
   ecore_idler_add(_e_comp_wl_cb_module_idle, cdata);

   if (comp->comp_type == E_PIXMAP_TYPE_X)
     {
        e_comp_wl_input_pointer_enabled_set(cdata, EINA_TRUE);
        e_comp_wl_input_keyboard_enabled_set(cdata, EINA_TRUE);
     }

   /* set compositor wayland data */
   comp->wl_comp_data = cdata;

   return EINA_TRUE;

input_err:
   e_comp_wl_data_manager_shutdown(cdata);
data_err:
comp_global_err:
   e_env_unset("WAYLAND_DISPLAY");
sock_err:
   wl_display_destroy(cdata->wl.disp);
disp_err:
   free(cdata);
   return EINA_FALSE;
}

/* public functions */
EAPI Eina_Bool 
e_comp_wl_init(void)
{
   /* set gl available if we have ecore_evas support */
   if (ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_WAYLAND_EGL) || 
       ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_OPENGL_DRM))
     e_comp_gl_set(EINA_TRUE);

   /* try to create a wayland compositor */
   if (!_e_comp_wl_compositor_create())
     {
        e_error_message_show(_("Enlightenment cannot create a Wayland Compositor!\n"));
        return EINA_FALSE;
     }

   /* try to init ecore_wayland */
   if (!ecore_wl_init(NULL))
     {
        e_error_message_show(_("Enlightenment cannot initialize Ecore_Wayland!\n"));
        return EINA_FALSE;
     }

   /* create hash to store clients */
   clients_win_hash = eina_hash_int64_new(NULL);

   /* add hooks to catch e_client events */
   e_client_hook_add(E_CLIENT_HOOK_NEW_CLIENT, _e_comp_wl_client_cb_new, NULL);
   e_client_hook_add(E_CLIENT_HOOK_DEL, _e_comp_wl_client_cb_del, NULL);

   return EINA_TRUE;
}

EAPI struct wl_signal 
e_comp_wl_surface_create_signal_get(E_Comp *comp)
{
   return comp->wl_comp_data->signals.surface.create;
}

/* internal functions */
EINTERN void 
e_comp_wl_shutdown(void)
{
   /* free the clients win hash */
   E_FREE_FUNC(clients_win_hash, eina_hash_free);

   /* shutdown ecore_wayland */
   ecore_wl_shutdown();
}

EINTERN struct wl_resource *
e_comp_wl_surface_create(struct wl_client *client, int version, uint32_t id)
{
   struct wl_resource *ret = NULL;

   if ((ret = wl_resource_create(client, &wl_surface_interface, version, id)))
     {
        DBG("Created Surface: %d", wl_resource_get_id(ret));
     }

   return ret;
}

EINTERN Eina_Bool 
e_comp_wl_surface_commit(E_Client *ec)
{
   E_Pixmap *ep;
   Eina_Rectangle *dmg;
   Eina_Tiler *src, *tmp;

   if (!(ep = ec->pixmap)) return EINA_FALSE;

   if (ec->comp_data->pending.new_attach)
     {
        /* TODO: buffer reference */

        if (ec->comp_data->pending.buffer)
          e_pixmap_resource_set(ep, ec->comp_data->pending.buffer->resource);
        else
          e_pixmap_resource_set(ep, NULL);

        e_pixmap_usable_set(ep, (ec->comp_data->pending.buffer != NULL));
     }

   /* mark the pixmap as dirty */
   e_pixmap_dirty(ep);

   /* check for any pending attachments */
   if (ec->comp_data->pending.new_attach)
     {
        /* check if the pending size is different than the client size */
        if ((ec->client.w != ec->comp_data->pending.w) || 
            (ec->client.h != ec->comp_data->pending.h))
          {
             /* if the client has a shell configure, call it */
             if ((ec->comp_data->shell.surface) && 
                 (ec->comp_data->shell.configure))
               ec->comp_data->shell.configure(ec->comp_data->shell.surface, 
                                              ec->client.x, ec->client.y,
                                              ec->comp_data->pending.w, 
                                              ec->comp_data->pending.h);
          }
     }

   /* check if we need to map this surface */
   if (ec->comp_data->pending.buffer)
     {
        /* if this surface is not mapped yet, map it */
        if (!ec->comp_data->mapped)
          {
             /* if the client has a shell map, call it */
             if ((ec->comp_data->shell.surface) && (ec->comp_data->shell.map))
               ec->comp_data->shell.map(ec->comp_data->shell.surface);
          }
     }
   else
     {
        /* no pending buffer to attach. unmap the surface */
        if (ec->comp_data->mapped)
          {
             /* if the client has a shell map, call it */
             if ((ec->comp_data->shell.surface) && (ec->comp_data->shell.unmap))
               ec->comp_data->shell.unmap(ec->comp_data->shell.surface);
          }
     }

   /* handle pending opaque */
   if (ec->comp_data->pending.opaque)
     {
        tmp = eina_tiler_new(ec->w, ec->h);
        eina_tiler_tile_size_set(tmp, 1, 1);
        eina_tiler_rect_add(tmp, 
                            &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});

        if ((src = eina_tiler_intersection(ec->comp_data->pending.opaque, tmp)))
          {
             Eina_Rectangle *rect;
             Eina_Iterator *itr;
             int i = 0;

             ec->shape_rects_num = 0;

             itr = eina_tiler_iterator_new(src);
             EINA_ITERATOR_FOREACH(itr, rect)
               ec->shape_rects_num += 1;

             ec->shape_rects = 
               malloc(sizeof(Eina_Rectangle) * ec->shape_rects_num);

             if (ec->shape_rects)
               {
                  EINA_ITERATOR_FOREACH(itr, rect)
                    {
                       ec->shape_rects[i] = *(Eina_Rectangle *)((char *)rect);

                       ec->shape_rects[i].x = rect->x;
                       ec->shape_rects[i].y = rect->y;
                       ec->shape_rects[i].w = rect->w;
                       ec->shape_rects[i].h = rect->h;

                       i++;
                    }
               }

             eina_iterator_free(itr);
             eina_tiler_free(src);
          }

        eina_tiler_free(tmp);
        eina_tiler_clear(ec->comp_data->pending.opaque);
     }

   /* commit any pending damages */
   if ((!ec->comp->nocomp) && (ec->frame))
     {
        EINA_LIST_FREE(ec->comp_data->pending.damages, dmg)
          {
             e_comp_object_damage(ec->frame, dmg->x, dmg->y, dmg->w, dmg->h);
             eina_rectangle_free(dmg);
          }
     }

   /* handle pending input */
   if (ec->comp_data->pending.input)
     {
        tmp = eina_tiler_new(ec->w, ec->h);
        eina_tiler_tile_size_set(tmp, 1, 1);
        eina_tiler_rect_add(tmp, 
                            &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});

        if ((src = eina_tiler_intersection(ec->comp_data->pending.input, tmp)))
          {
             Eina_Rectangle *rect;
             Eina_Iterator *itr;
             int i = 0;

             ec->shape_input_rects_num = 0;

             itr = eina_tiler_iterator_new(src);
             EINA_ITERATOR_FOREACH(itr, rect)
               ec->shape_input_rects_num += 1;

             ec->shape_input_rects = 
               malloc(sizeof(Eina_Rectangle) * ec->shape_input_rects_num);

             if (ec->shape_input_rects)
               {
                  EINA_ITERATOR_FOREACH(itr, rect)
                    {
                       ec->shape_input_rects[i] = 
                         *(Eina_Rectangle *)((char *)rect);

                       ec->shape_input_rects[i].x = rect->x;
                       ec->shape_input_rects[i].y = rect->y;
                       ec->shape_input_rects[i].w = rect->w;
                       ec->shape_input_rects[i].h = rect->h;

                       i++;
                    }
               }

             eina_iterator_free(itr);
             eina_tiler_free(src);
          }

        eina_tiler_free(tmp);
        eina_tiler_clear(ec->comp_data->pending.opaque);
     }

   return EINA_TRUE;
}
