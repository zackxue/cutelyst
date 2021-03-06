/*
 * Copyright (C) 2013-2014 Daniel Nicoletti <dantti12@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <uwsgi.h>

#include "plugin.h"

int uwsgi_cutelyst_init(void);
void uwsgi_cutelyst_post_fork(void);
int uwsgi_cutelyst_request(struct wsgi_request *);
void uwsgi_cutelyst_init_apps(void);
void uwsgi_cutelyst_on_load(void);
void uwsgi_cutelyst_atexit(void);
void uwsgi_cutelyst_master_cleanup(void);

struct uwsgi_option uwsgi_cutelyst_options[] = {

{"cutelyst-app", required_argument, 0, "loads the Cutelyst Application", uwsgi_opt_set_str, &options.app, 0},
{"cutelyst-reload", no_argument, 0, "auto reloads the application when app file changes", uwsgi_opt_true, &options.reload, 0},
{0, 0, 0, 0, 0, 0, 0},

};

struct uwsgi_plugin cutelyst_plugin = {

    .name = "cutelyst",
    .modifier1 = CUTELYST_MODIFIER1,
    .on_load = uwsgi_cutelyst_on_load,
    .init = uwsgi_cutelyst_init,
    .post_fork = uwsgi_cutelyst_post_fork,
    .request = uwsgi_cutelyst_request,
    .init_apps = uwsgi_cutelyst_init_apps,
    .options = uwsgi_cutelyst_options,
    .atexit = uwsgi_cutelyst_atexit,

};
