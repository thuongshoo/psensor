/*
 * Copyright (C) 2010-2016 jeanfi@gmail.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <notify_cmd.h>
#include <cfg.h>

void notify_cmd(struct psensor *s)
{
	char *script, *v, *cmd;
	int ret;

	script = config_get_notif_script();

	if (script) {
		v = psensor_current_value_to_str(s, 1);

		int result = asprintf(&cmd, "%s \"%s\" %s", script, s->id, v);
		if (result == -1) {
			log_functionname("cannot allote memory when trying to execute cmd: %s", cmd);
			if (v) free(v);
			free(script);
			return;
		}

		log_functionname("execute cmd: %s", cmd);

		ret = system(cmd);

		log_functionname("cmd returns: %d", ret);

		free(cmd);
		free(v);
		free(script);
	}
}

