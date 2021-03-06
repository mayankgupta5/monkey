/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Monkey HTTP Server
 *  ==================
 *  Copyright 2001-2015 Monkey Software LLC <eduardo@monkey.io>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef MK_PLUGIN_STAGE_H
#define MK_PLUGIN_STAGE_H

static inline int mk_plugin_stage_run_10(int socket,
                                         struct sched_connection *conx)
{
    int ret;
    struct mk_list *head;
    struct mk_plugin_stage *stage;

    mk_list_foreach(head, &mk_config->stage10_handler) {
        stage = mk_list_entry(head, struct mk_plugin_stage, _head);
        ret = stage->stage10(socket, conx);
        switch (ret) {
        case MK_PLUGIN_RET_CLOSE_CONX:
            MK_TRACE("return MK_PLUGIN_RET_CLOSE_CONX");
            return MK_PLUGIN_RET_CLOSE_CONX;
        }
    }

    return -1;
}

static inline int mk_plugin_stage_run_20(struct mk_http_session *cs,
                                         struct mk_http_request *sr)
{
    int ret;
    struct mk_list *head;
    struct mk_plugin_stage *stage;

    mk_list_foreach(head, &mk_config->stage20_handler) {
        stage = mk_list_entry(head, struct mk_plugin_stage, _head);
        ret = stage->stage20(cs, sr);
        switch (ret) {
        case MK_PLUGIN_RET_CLOSE_CONX:
            MK_TRACE("return MK_PLUGIN_RET_CLOSE_CONX");
            return MK_PLUGIN_RET_CLOSE_CONX;
        }
    }

    return -1;
}

static inline int mk_plugin_stage_run_30(struct mk_http_session *cs,
                                         struct mk_http_request *sr)
{
    int ret;
    struct mk_list *head;
    struct mk_plugin_stage *stage;

    mk_list_foreach(head, &mk_config->stage30_handler) {
        stage = mk_list_entry(head, struct mk_plugin_stage, _head);
        ret = stage->stage30(stage->plugin, cs, sr);
        switch (ret) {
        case MK_PLUGIN_RET_NOT_ME:
            break;
        case MK_PLUGIN_RET_END:
            mk_bug(sr->headers.sent == MK_FALSE);
            return ret;
        case MK_PLUGIN_RET_CLOSE_CONX:
        case MK_PLUGIN_RET_CONTINUE:
            return ret;
        default:
            mk_err("Plugin returns invalid value %i", ret);
            exit(EXIT_FAILURE);
        }
    }

    return -1;
}

static inline int mk_plugin_stage_run_40(struct mk_http_session *cs,
                                         struct mk_http_request *sr)
{
    struct mk_list *head;
    struct mk_plugin_stage *stage;

    mk_list_foreach(head, &mk_config->stage40_handler) {
        stage = mk_list_entry(head, struct mk_plugin_stage, _head);
        stage->stage40(cs, sr);
    }

    return -1;
}

static inline int mk_plugin_stage_run_50(int socket)
{
    int ret;

    struct mk_list *head;
    struct mk_plugin_stage *stage;

    mk_list_foreach(head, &mk_config->stage50_handler) {
        stage = mk_list_entry(head, struct mk_plugin_stage, _head);
        ret = stage->stage50(socket);
        switch (ret) {
        case MK_PLUGIN_RET_NOT_ME:
            break;
        case MK_PLUGIN_RET_CONTINUE:
            return MK_PLUGIN_RET_CONTINUE;
        }
    }

    return -1;
}

#endif
