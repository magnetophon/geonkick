/**
 * File name: gkick_kack.h
 * Project: Geonkick (A kick synthesizer)
 *
 * Copyright (C) 2017 Iurie Nistor <http://geontime.com>
 *
 * This file is part of Geonkick.
 *
 * GeonKick is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "gkick_jack.h"
#include "oscillator.h"

int
gkick_jack_process_callback(jack_nframes_t nframes,
			    void *arg)
{
        struct gkick_jack *jack = (struct gkick_jack*)arg;
	jack_default_audio_sample_t *buffer;
        if (gkick_jack_get_output_buffers(jack, &buffer, nframes) != GEONKICK_OK) {
                gkick_log_error("can't get output jack buffers");
                return 0;
        }
        void* port_buf = jack_port_get_buffer(jack->midi_in_port, nframes);

	jack_midi_event_t event;
	jack_nframes_t events_count = jack_midi_get_event_count(port_buf);
        jack_nframes_t event_index  = 0;
        if (events_count > 0)
                jack_midi_event_get(&event, port_buf, event_index);

        for (size_t i = 0; i < nframes; i++) {
                while (event.time == i && event_index < events_count) {
			struct gkick_note_info note;
                        memset(&note, 0, sizeof(struct gkick_note_info));
                        gkick_jack_get_note_info(&event, &note);
                        if (note.state == GKICK_KEY_STATE_PRESSED
                            || note.state == GKICK_KEY_STATE_RELEASED) {
                                gkick_mixer_key_pressed(jack->mixer, &note);
                        }
                        event_index++;
                        if (event_index < events_count)
                                jack_midi_event_get(&event, port_buf, event_index);
                }

		gkick_real val;
                gkick_mixer_get_frame(jack->mixer, 0, &val);
                val *= 0.1f;
                if (val > 0.1f)
                        val = 0.1f;
                buffer[i] = (jack_default_audio_sample_t)val;
        }

        return 0;
}

jack_nframes_t
gkick_jack_sample_rate(struct gkick_jack *jack)
{
        jack_nframes_t sample_rate;
        if (jack == NULL) {
                gkick_log_error("wrong arguments");
                return 0;
        }

        gkick_jack_lock(jack);
        sample_rate = jack->sample_rate;
        gkick_jack_unlock(jack);

        return sample_rate;
}

enum geonkick_error
gkick_jack_get_output_buffers(struct gkick_jack *jack,
                              jack_default_audio_sample_t **channel_buf,
                              jack_nframes_t nframes)
{
        if (jack->output_port == NULL) {
                gkick_log_error("output ports are undefined");
                *channel_buf = NULL;
                return GEONKICK_ERROR;
        } else {
                *channel_buf = (jack_default_audio_sample_t*)jack_port_get_buffer(jack->output_port,
                                                                                  nframes);
                if (*channel_buf == NULL)
                        return GEONKICK_ERROR;
        }

        return GEONKICK_OK;
}

void
gkick_jack_get_note_info(jack_midi_event_t *event,
                         struct gkick_note_info *note)
{
        if (event == NULL || note == NULL)
                return;

        note->state = GKICK_KEY_STATE_DEFAULT;
        if (((*(event->buffer) & 0xf0)) == 0x90) {
                note->state = GKICK_KEY_STATE_PRESSED;
                note->channel     = *(event->buffer) & 0x0f;
                note->note_number = event->buffer[1];
                note->velocity    = event->buffer[2];
        } else if(((*(event->buffer) & 0xf0)) == 0x80) {
                note->state       = GKICK_KEY_STATE_RELEASED;
                note->channel     = *(event->buffer) & 0x0f;
                note->note_number = event->buffer[1];
                note->velocity    = event->buffer[2];
        }
}

jack_port_t*
gkick_jack_get_midi_in_port(struct gkick_jack *jack)
{
        jack_port_t *port;

        if (jack == NULL) {
                gkick_log_error("wrong arguments");
                return NULL;
        }

        gkick_jack_lock(jack);
        port = jack->midi_in_port;
        gkick_jack_unlock(jack);

        return port;
}

int gkick_jack_srate_callback(jack_nframes_t nframes,
                              void *arg)
{
	return 0;
}

enum geonkick_error
gkick_jack_enable_midi_in(struct gkick_jack *jack,
                          const char *name)
{
        const char *midi_name;
        enum geonkick_error error;

        if (jack == NULL) {
                gkick_log_error("wrong arguments");
                return GEONKICK_ERROR;
        }

        if (name == NULL)
                midi_name = "midi_in";
        else
                midi_name = name;

        error = GEONKICK_OK;
        gkick_jack_lock(jack);
        if (jack->midi_in_port == NULL) {
                jack->midi_in_port = jack_port_register(jack->client, midi_name,
                                                JACK_DEFAULT_MIDI_TYPE,
                                                JackPortIsInput, 0);
                if (jack->midi_in_port == NULL) {
                        gkick_log_error("can't register port: %s",
                                           midi_name);
                        error = GEONKICK_ERROR;
                }
        } else {
                gkick_log_warning("midi in is already set");
        }
        gkick_jack_unlock(jack);
        return error;
}

enum geonkick_error
gkick_jack_create_output_ports(struct gkick_jack *jack)
{
        enum geonkick_error error;
        if (jack == NULL) {
                gkick_log_error("wrong arguments");
                return GEONKICK_ERROR;
        }

        error = GEONKICK_OK;
        gkick_jack_lock(jack);
        if (jack->output_port == NULL) {
                jack->output_port = jack_port_register(jack->client, "audio_out",
                                                       JACK_DEFAULT_AUDIO_TYPE,
                                                       JackPortIsOutput, 0);
                if (jack->output_port == NULL) {
                        gkick_log_error("can't register output ports");
                        error = GEONKICK_ERROR;
                }
        } else {
                gkick_log_warning("output ports already created");
        }
        gkick_jack_unlock(jack);
        return error;
}

enum geonkick_error
gkick_create_jack(struct gkick_jack **jack,
                  struct gkick_mixer *mixer)
{
        if (jack == NULL || mixer == NULL) {
                gkick_log_error("wrong arguments");
                return GEONKICK_ERROR;
        }

        *jack = (struct gkick_jack*)calloc(1, sizeof(struct gkick_jack));
        if (*jack == NULL)
                return GEONKICK_ERROR;
        (*jack)->sample_rate = GEONKICK_SAMPLE_RATE;

        if (pthread_mutex_init(&(*jack)->lock, NULL) != 0) {
                gkick_log_error("error on init mutex");
                gkick_jack_free(jack);
                return GEONKICK_ERROR;
        }

        (*jack)->client = jack_client_open(GEONKICK_NAME,
                                           JackNoStartServer,
                                           NULL);
        if ((*jack)->client == NULL) {
                gkick_log_error("can't create jack client");
                gkick_jack_free(jack);
                return GEONKICK_ERROR;
        }

        jack_set_process_callback((*jack)->client,
                                  gkick_jack_process_callback,
                                  (void*)(*jack));

        if (gkick_jack_create_output_ports(*jack) != GEONKICK_OK) {
                gkick_log_error("can't create output ports");
                gkick_jack_free(jack);
                return GEONKICK_ERROR;
        }
        (*jack)->mixer = mixer;

        gkick_jack_enable_midi_in(*jack, "midi_in");
                if (jack_activate((*jack)->client) != 0) {
                gkick_log_error("cannot activate client");
                gkick_jack_free(jack);
                return GEONKICK_ERROR;
        }

	return GEONKICK_OK;
}

int
gkick_jack_is_midi_in_enabled(struct gkick_jack *jack)
{
        int enabled;

        if (jack == NULL) {
                gkick_log_error("wrong arugment");
                return 0;
        }

        gkick_jack_lock(jack);
        enabled = (jack->midi_in_port != NULL) ? 1 : 0;
        gkick_jack_unlock(jack);

        return enabled;
}

void
gkick_jack_free(struct gkick_jack **jack)
{
        if (jack != NULL && *jack != NULL) {
                if ((*jack)->client != NULL) {
                        jack_deactivate((*jack)->client);
                        if ((*jack)->output_port != NULL) {
                                jack_port_unregister((*jack)->client,
                                                     (*jack)->output_port);
                        }
                        jack_port_unregister((*jack)->client,
                                             (*jack)->output_port);
                        jack_client_close((*jack)->client);
                }

                free(*jack);
                *jack = NULL;
        }
}

void
gkick_jack_lock(struct gkick_jack *jack)
{
        if (jack != NULL)
                pthread_mutex_lock(&jack->lock);
}

void
gkick_jack_unlock(struct gkick_jack *jack)
{
        if (jack != NULL)
                pthread_mutex_unlock(&jack->lock);
}
