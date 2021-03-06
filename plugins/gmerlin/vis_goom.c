/*****************************************************************
 
  vis_goom.c
 
  Copyright (c) 2007 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <string.h>
#include <math.h>

#include <config.h>
#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <gmerlin/utils.h>
#include <gmerlin/log.h>

#include <goom.h>

#define LOG_DOMAIN "vis_goom"

#define BLUR_MODE_GAUSS      0
#define BLUR_MODE_TRIANGULAR 1
#define BLUR_MODE_BOX        2


typedef struct
  {
  gavl_video_format_t video_format;
  gavl_audio_format_t audio_format;

  int16_t audio_data[2][512];
  gavl_video_frame_t * video_frame;
  gavl_audio_frame_t * audio_frame;
  
  PluginInfo * goom;

  gavl_audio_sink_t * asink;
  gavl_video_source_t * vsrc;
  
  } goom_priv_t;

static void * create_goom()
  {
  goom_priv_t * ret;
  ret = calloc(1, sizeof(*ret));
  
  ret->video_frame = gavl_video_frame_create(NULL);
  ret->audio_frame = gavl_audio_frame_create(NULL);

  ret->audio_frame->channels.s_16[0] = ret->audio_data[0];
  ret->audio_frame->channels.s_16[1] = ret->audio_data[1];
  
  return ret;
  }

static void close_goom(void * priv)
  {
  goom_priv_t * vp = priv;
  
  if(vp->audio_frame)
    {
    gavl_audio_frame_null(vp->audio_frame);
    gavl_audio_frame_destroy(vp->audio_frame);
    vp->audio_frame = NULL;
    }
  if(vp->video_frame)
    {
    gavl_video_frame_null(vp->video_frame);
    gavl_video_frame_destroy(vp->video_frame);
    vp->video_frame = NULL;
    }

  if(vp->asink)
    {
    gavl_audio_sink_destroy(vp->asink);
    vp->asink = NULL;
    }
  
  if(vp->vsrc)
    {
    gavl_video_source_destroy(vp->vsrc);
    vp->vsrc = NULL;
    }
  
  if(vp->goom)
    {
    goom_close(vp->goom);
    vp->goom = NULL;
    }
  
  }

static void destroy_goom(void * priv) 
  {
  goom_priv_t * vp = priv;
  
  close_goom(vp);
  
  free(vp);
  }

#if 0

static bg_parameter_info_t parameters[] =
  {
    { /* End of parameters */ },
  };

static bg_parameter_info_t * get_parameters_goom(void * priv)
  {
  return parameters;
  }

static void
set_parameter_goom(void * priv, const char * name,
                    const gavl_value_t * val)
  {
  }
#endif

static gavl_audio_sink_t * get_sink_goom(void * priv)
  {
  goom_priv_t * vp = priv;
  return vp->asink;
  }

static gavl_video_source_t * get_src_goom(void * priv)
  {
  goom_priv_t * vp = priv;
  return vp->vsrc;
  }

static gavl_source_status_t draw_frame_goom(void * priv, gavl_video_frame_t ** frame)
  {
  uint32_t * pixels;
  goom_priv_t * vp = priv;
  
  pixels =
    goom_update(vp->goom, vp->audio_data, 0, -1.0 /* FPS */,
                (char *)0 /* songTitle */, (char*)0 /* message */ );
  
  vp->video_frame->planes[0] = (uint8_t*)pixels;
  vp->video_frame->strides[0] = 4 * vp->video_format.image_width;
  gavl_video_frame_copy(&vp->video_format,
                        *frame, vp->video_frame);
  return GAVL_SOURCE_OK;
  }

static gavl_sink_status_t put_frame_goom(void * priv, gavl_audio_frame_t * frame)
  {
  goom_priv_t * vp;
  vp = (goom_priv_t *)priv;
  gavl_audio_frame_copy(&vp->audio_format, vp->audio_frame, frame,
                        0, 0, vp->audio_format.samples_per_frame,
                        frame->valid_samples);
  return GAVL_SINK_OK;
  }



static int
open_goom(void * priv,
          gavl_audio_format_t * audio_format,
          gavl_video_format_t * video_format)
  {
  goom_priv_t * vp = priv;

  /* Adjust video format */
  
  if(video_format->image_width < 16)
    video_format->image_width = 16;
  if(video_format->image_height < 16)
    video_format->image_height = 16;

  gavl_video_format_set_frame_size(video_format, 1, 1);
  
  video_format->pixel_width  = 1;
  video_format->pixel_height  = 1;
  video_format->pixelformat = GAVL_RGB_32;
  
  /* Adjust audio format */
  
  audio_format->sample_format = GAVL_SAMPLE_S16;
  audio_format->interleave_mode = GAVL_INTERLEAVE_NONE;
  audio_format->samples_per_frame = 512;
  
  audio_format->num_channels = 2;
  audio_format->channel_locations[0] = GAVL_CHID_NONE;
  gavl_set_channel_setup(audio_format);
  
  gavl_video_format_copy(&vp->video_format, video_format);
  gavl_audio_format_copy(&vp->audio_format, audio_format);
  
  /* Initialize goom */

  if(vp->goom)
    goom_close(vp->goom);
  vp->goom = goom_init(vp->video_format.image_width,
                       vp->video_format.image_height);

  vp->asink = gavl_audio_sink_create(NULL, put_frame_goom, vp, &vp->audio_format);

  vp->vsrc = gavl_video_source_create(draw_frame_goom, vp, 0,
                                      &vp->video_format);

#if 0  
  fprintf(stderr, "Open goom:\n");
  gavl_audio_format_dump(&vp->audio_format);
  gavl_video_format_dump(&vp->video_format);
#endif
  
  return 1;
  }


const bg_visualization_plugin_t the_plugin = 
  {
    common:
    {
      BG_LOCALE,
      .name =      "vis_goom",
      .long_name = TRS("Goom"),
      .description = TRS("Goom plugin"),
      .type =     BG_PLUGIN_VISUALIZATION,
      .flags =    0,
      .create =   create_goom,
      .destroy =   destroy_goom,
      //      get_parameters:   get_parameters_goom,
      //      set_parameter:    set_parameter_goom,
      priority:         1,
    },
    .open = open_goom,

    .get_source =  get_src_goom,
    .get_sink = get_sink_goom,
    
    .close = close_goom
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
