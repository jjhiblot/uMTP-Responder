/*
 * uMTP Responder
 * Copyright (c) 2018 Viveris Technologies
 *
 * uMTP Responder is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3.0 of the License, or (at your option) any later version.
 *
 * uMTP Responder is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 3 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with uMTP Responder; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file   inotify.c
 * @brief  inotify file system events handler.
 * @author Jean-François DEL NERO <Jean-Francois.DELNERO@viveris.fr>
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <errno.h>

#include <poll.h>
#include <sys/inotify.h>

#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>

#include "logs_out.h"

#include "mtp_helpers.h"

#include "fs_handles_db.h"

#include "mtp.h"
#include "mtp_datasets.h"

#include "usb_gadget_fct.h"

#include "inotify.h"

#include "mtp_constant.h"

#define INOTIFY_RD_BUF_SIZE ( 1024 * (  ( sizeof (struct inotify_event) ) + 16 ) )

static int get_file_info(mtp_ctx * ctx, const struct inotify_event *event, fs_entry * entry, filefoundinfo * fileinfo, int deleted)
{
	char * path;
	char * tmp_path;

	if( entry && fileinfo )
	{
		PRINT_DEBUG( "Entry %x - %s", entry->handle, entry->name );

		path = build_full_path( ctx->fs_db, mtp_get_storage_root( ctx, entry->storage_id), entry);
		if( path )
		{
			tmp_path = malloc( strlen(path) + strlen(event->name) + 3 );
			if( tmp_path )
			{
				strcpy( tmp_path, path );
				strcat( tmp_path, "/" );
				strcat( tmp_path, event->name );

				if( !deleted )
				{
					fs_entry_stat( tmp_path, fileinfo );
				}
				else
				{
					fileinfo->isdirectory = 0;
					fileinfo->size = 0;
					strncpy( fileinfo->filename, event->name, 256 );
				}

				free( tmp_path );

				return 1;
			}
			free( path );
		}
	}

	return 0;
}

void* inotify_thread(void* arg)
{
	mtp_ctx * ctx;
	int i,length;
	fs_entry * entry;
	fs_entry * deleted_entry;
	fs_entry * new_entry;
	filefoundinfo fileinfo;
	uint32_t handle[3];
	char buffer[INOTIFY_RD_BUF_SIZE] __attribute__ ((aligned(__alignof__(struct inotify_event))));
	const struct inotify_event *event;

	ctx = (mtp_ctx *)arg;

	for (;;)
	{
		length = read(ctx->inotify_fd, buffer, sizeof buffer);
		if (length >= 0)
		{
			i = 0;

			while ( i < length )
			{
				event = ( struct inotify_event * ) &buffer[ i ];
				if ( event->len )
				{
					if ( event->mask & IN_CREATE )
					{
						entry = get_entry_by_wd( ctx->fs_db, event->wd );
						if ( get_file_info( ctx, event, entry, &fileinfo, 0 ) )
						{
							new_entry = add_entry( ctx->fs_db, &fileinfo, entry->handle, entry->storage_id );

							// Send an "ObjectAdded" (0x4002) MTP event message with the entry handle.
							handle[0] = new_entry->handle;

							mtp_push_event( ctx, MTP_EVENT_OBJECT_ADDED, 1, (uint32_t *)&handle );

							PRINT_DEBUG( "inotify_thread : Entry %s created (Handle 0x%.8X)", event->name, new_entry->handle );
						}
					}
					else
					{
						if ( event->mask & IN_DELETE )
						{
							entry = get_entry_by_wd( ctx->fs_db, event->wd );
							if ( get_file_info( ctx, event, entry, &fileinfo, 1 ) )
							{
								deleted_entry = search_entry(ctx->fs_db, &fileinfo, entry->handle, entry->storage_id);
								if( deleted_entry )
								{
									deleted_entry->flags |= ENTRY_IS_DELETED;

									// Send an "ObjectRemoved" (0x4003) MTP event message with the entry handle.
									handle[0] = new_entry->handle;
									mtp_push_event( ctx, MTP_EVENT_OBJECT_REMOVED, 1, (uint32_t *)&handle );

									PRINT_DEBUG( "inotify_thread : Entry %s deleted (Handle 0x%.8X)", event->name, deleted_entry->handle);
								}
							}
						}
					}

					i +=  (( sizeof (struct inotify_event) ) + event->len);
				}
			}
		}
	}

	return NULL;

}

int inotify_handler_init( mtp_ctx * ctx )
{
	if( ctx )
	{
		ctx->inotify_fd = inotify_init1(0x00);

		PRINT_DEBUG("init_inotify_handler : inotify_fd = %d", ctx->inotify_fd);

		pthread_create(&ctx->inotify_thread, NULL, inotify_thread, ctx);

		return 1;
	}

	return 0;
}

int inotify_handler_deinit( mtp_ctx * ctx )
{
	if( ctx )
	{
		if( ctx->inotify_fd != -1 )
		{
			close( ctx->inotify_fd );
			ctx->inotify_fd = -1;
		}

		return 1;
	}

	return 0;
}

int inotify_handler_addwatch( mtp_ctx * ctx, char * path )
{
	if( ctx->inotify_fd != -1 )
	{
		return inotify_add_watch( ctx->inotify_fd, path, IN_CREATE | IN_DELETE );
	}

	return -1;
}

