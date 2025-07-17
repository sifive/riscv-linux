/*
 *
 * Copyright (C) 2021 ESWIN, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _ES_AUDIO_PROC_H_
#define _ES_AUDIO_PROC_H_

int audio_proc_module_init(void);
void audio_proc_module_exit(void);

int audio_create_procfs(void);
void audio_remove_procfs(void);
#endif
