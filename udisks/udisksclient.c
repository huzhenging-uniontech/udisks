/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2007-2010 David Zeuthen <zeuthen@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include <string.h>

//#include <glib/gi18n-lib.h>
#define _(x) x


#include "udisksclient.h"
#include "udiskserror.h"
#include "udisks-generated.h"

/**
 * SECTION:udisksclient
 * @title: UDisksClient
 * @short_description: UDisks Client
 *
 * #UDisksClient is used for accessing the UDisks service from a
 * client program.
 */

G_LOCK_DEFINE_STATIC (init_lock);

/**
 * UDisksClient:
 *
 * The #UDisksClient structure contains only private data and should
 * only be accessed using the provided API.
 */
struct _UDisksClient
{
  GObject parent_instance;

  gboolean is_initialized;
  GError *initialization_error;

  GDBusProxyManager *proxy_manager;
};

typedef struct
{
  GObjectClass parent_class;
} UDisksClientClass;

enum
{
  PROP_0,
  PROP_PROXY_MANAGER
};

static void initable_iface_init       (GInitableIface      *initable_iface);
static void async_initable_iface_init (GAsyncInitableIface *async_initable_iface);

G_DEFINE_TYPE_WITH_CODE (UDisksClient, udisks_client, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                         );

static void
udisks_client_finalize (GObject *object)
{
  UDisksClient *client = UDISKS_CLIENT (object);

  if (client->initialization_error != NULL)
    g_error_free (client->initialization_error);

  g_object_unref (client->proxy_manager);

  G_OBJECT_CLASS (udisks_client_parent_class)->finalize (object);
}

static void
udisks_client_init (UDisksClient *client)
{
  static volatile GQuark udisks_error_domain = 0;
  /* this will force associating errors in the UDISKS_ERROR error domain
   * with org.freedesktop.UDisks.Error.* errors via g_dbus_error_register_error_domain().
   */
  udisks_error_domain = UDISKS_ERROR;
  udisks_error_domain; /* shut up -Wunused-but-set-variable */
}

static void
udisks_client_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  UDisksClient *client = UDISKS_CLIENT (object);

  switch (prop_id)
    {
    case PROP_PROXY_MANAGER:
      g_value_set_object (value, udisks_client_get_proxy_manager (client));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
udisks_client_class_init (UDisksClientClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize     = udisks_client_finalize;
  gobject_class->get_property = udisks_client_get_property;

  /**
   * UDisksClient:proxy-manager:
   *
   * The #GDBusProxyManager used by the #UDisksClient instance.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PROXY_MANAGER,
                                   g_param_spec_object ("proxy-manager",
                                                        "proxy manager",
                                                        "The #GDBusProxyManager used by the UDisksClient",
                                                        G_TYPE_DBUS_PROXY_MANAGER,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));
}

/**
 * udisks_client_new:
 * @cancellable: A #GCancellable or %NULL.
 * @callback: Function that will be called when the result is ready.
 * @user_data: Data to pass to @callback.
 *
 * Asynchronously gets a #UDisksClient. When the operation is
 * finished, @callback will be invoked in the <link
 * linkend="g-main-context-push-thread-default">thread-default main
 * loop</link> of the thread you are calling this method from.
 */
void
udisks_client_new (GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data)
{
  g_async_initable_new_async (UDISKS_TYPE_CLIENT,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              NULL);
}

/**
 * udisks_client_new_finish:
 * @res: A #GAsyncResult.
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with udisks_client_new().
 *
 * Returns: A #UDisksClient or %NULL if @error is set. Free with
 * g_object_unref() when done with it.
 */
UDisksClient *
udisks_client_new_finish (GAsyncResult        *res,
                          GError             **error)
{
  GObject *ret;
  GObject *source_object;
  source_object = g_async_result_get_source_object (res);
  ret = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object), res, error);
  g_object_unref (source_object);
  if (ret != NULL)
    return UDISKS_CLIENT (ret);
  else
    return NULL;
}

/**
 * udisks_client_new_sync:
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @error: (allow-none): Return location for error or %NULL.
 *
 * Synchronously gets a #UDisksClient for the local system.
 *
 * Returns: A #UDisksClient or %NULL if @error is set. Free with
 * g_object_unref() when done with it.
 */
UDisksClient *
udisks_client_new_sync (GCancellable  *cancellable,
                        GError       **error)
{
  GInitable *ret;
  ret = g_initable_new (UDISKS_TYPE_CLIENT,
                        cancellable,
                        error,
                        NULL);
  if (ret != NULL)
    return UDISKS_CLIENT (ret);
  else
    return NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
initable_init (GInitable     *initable,
               GCancellable  *cancellable,
               GError       **error)
{
  UDisksClient *client = UDISKS_CLIENT (initable);
  gboolean ret;

  ret = FALSE;

  /* This method needs to be idempotent to work with the singleton
   * pattern. See the docs for g_initable_init(). We implement this by
   * locking.
   */
  G_LOCK (init_lock);
  if (client->is_initialized)
    {
      if (client->proxy_manager != NULL)
        ret = TRUE;
      else
        g_assert (client->initialization_error != NULL);
      goto out;
    }
  g_assert (client->initialization_error == NULL);

  client->proxy_manager = udisks_proxy_manager_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                                 G_DBUS_PROXY_MANAGER_FLAGS_NONE,
                                                                 "org.freedesktop.UDisks2",
                                                                 "/org/freedesktop/UDisks2",
                                                                 cancellable,
                                                                 &client->initialization_error);
  if (client->proxy_manager == NULL)
    goto out;

  ret = TRUE;

out:
  client->is_initialized = TRUE;
  if (!ret)
    {
      g_assert (client->initialization_error != NULL);
      g_propagate_error (error, g_error_copy (client->initialization_error));
    }
  G_UNLOCK (init_lock);
  return ret;
}

static void
initable_iface_init (GInitableIface      *initable_iface)
{
  initable_iface->init = initable_init;
}

static void
async_initable_iface_init (GAsyncInitableIface *async_initable_iface)
{
  /* Use default implementation (e.g. run GInitable code in a thread) */
}

/**
 * udisks_client_get_proxy_manager:
 * @client: A #UDisksClient.
 *
 * Gets the #GDBusProxyManager used by @client.
 *
 * Returns: (transfer none): A #GDBusProxyManager. Do not free, the
 * instance is owned by @client.
 */
GDBusProxyManager *
udisks_client_get_proxy_manager (UDisksClient        *client)
{
  g_return_val_if_fail (UDISKS_IS_CLIENT (client), NULL);
  return client->proxy_manager;
}
