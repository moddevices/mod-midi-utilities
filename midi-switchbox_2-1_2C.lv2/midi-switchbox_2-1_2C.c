/* MIDI switchbox2*/

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>

#include <stdbool.h>
#include <stdlib.h>

typedef enum {
    PORT_CONTROL_TARGET = 0,
    PORT_MIDI_IN1,
    PORT_MIDI_IN2,
    PORT_MIDI_IN3,
    PORT_MIDI_IN4,
    PORT_MIDI_OUT1,
    PORT_MIDI_OUT2
} PortEnum;

typedef enum {
    SOURCE_1_AND_2 = 0,
    SOURCE_3_AND_4,
} SourceEnum;

typedef struct {

    int previous_source;

    // URIDs
    LV2_URID urid_midiEvent;

    // control ports
    const float* port_source;

    // atom ports
    const LV2_Atom_Sequence* p_port_events_in1;
    const LV2_Atom_Sequence* p_port_events_in2;

    const LV2_Atom_Sequence* port_events_in1;
    const LV2_Atom_Sequence* port_events_in2;
    const LV2_Atom_Sequence* port_events_in3;
    const LV2_Atom_Sequence* port_events_in4;
    LV2_Atom_Sequence* port_events_out1;
    LV2_Atom_Sequence* port_events_out2;
} Data;

// Struct for a 3 byte MIDI event
typedef struct {
    LV2_Atom_Event event;
    uint8_t        msg[3];
} LV2_Atom_MIDI;

static LV2_Handle instantiate(const LV2_Descriptor*     descriptor,
                              double                    rate,
                              const char*               path,
                              const LV2_Feature* const* features)
{
    Data* self = (Data*)calloc(1, sizeof(Data));

    // Get host features
    const LV2_URID_Map* map = NULL;

    for (int i = 0; features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_URID__map)) {
            map = (const LV2_URID_Map*)features[i]->data;
            break;
        }
    }
    if (!map) {
        free(self);
        return NULL;
    }

    // Map URIs
    self->urid_midiEvent = map->map(map->handle, LV2_MIDI__MidiEvent);

    self->previous_source = 0;
    self->p_port_events_in1 = NULL;
    self->p_port_events_in2 = NULL;

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
    Data* self = (Data*)instance;

    switch (port)
    {
    case PORT_CONTROL_TARGET:
            self->port_source = (const float*)data;
            break;
    case PORT_MIDI_IN1:
            self->port_events_in1 = (const LV2_Atom_Sequence*)data;
            break;
    case PORT_MIDI_IN2:
            self->port_events_in2 = (const LV2_Atom_Sequence*)data;
            break;
    case PORT_MIDI_IN3:
            self->port_events_in3 = (const LV2_Atom_Sequence*)data;
            break;
    case PORT_MIDI_IN4:
            self->port_events_in4 = (const LV2_Atom_Sequence*)data;
            break;
    case PORT_MIDI_OUT1:
            self->port_events_out1 = (LV2_Atom_Sequence*)data;
            break;
    case PORT_MIDI_OUT2:
            self->port_events_out2 = (LV2_Atom_Sequence*)data;
            break;
    }
}

static void activate(LV2_Handle instance)
{
}

static void run(LV2_Handle instance, uint32_t sample_count)
{
    Data* self = (Data*)instance;

    const int source = (int)*self->port_source;

    // Get the capacity
    const uint32_t out_capacity1 = self->port_events_out1->atom.size;
    const uint32_t out_capacity2 = self->port_events_out2->atom.size;
    // Write an empty Sequence header to the output
    lv2_atom_sequence_clear(self->port_events_out1);
    lv2_atom_sequence_clear(self->port_events_out2);

    // LV2 is so nice...
    self->port_events_out1->atom.type = self->port_events_in1->atom.type;
    self->port_events_out2->atom.type = self->port_events_in2->atom.type;

    // Send note-offs if source port changed
    if (self->previous_source != source)
    {
        LV2_Atom_MIDI msg;
        memset(&msg, 0, sizeof(LV2_Atom_MIDI));

        msg.event.body.size = 3;
        msg.event.body.type = self->urid_midiEvent;
        msg.msg[2] = 0;

        for (uint32_t c = 0; c < 0xf; ++c) {
            msg.msg[0] = 0xb0 | c;
            msg.msg[1] = 0x40; // sustain pedal
            lv2_atom_sequence_append_event(self->port_events_out1,
                    out_capacity1,
                    (LV2_Atom_Event*)&msg);
            lv2_atom_sequence_append_event(self->port_events_out2,
                    out_capacity2,
                    (LV2_Atom_Event*)&msg);
            msg.msg[1] = 0x7b; // all notes off
            lv2_atom_sequence_append_event(self->port_events_out1,
                    out_capacity1,
                    (LV2_Atom_Event*)&msg);
            lv2_atom_sequence_append_event(self->port_events_out2,
                    out_capacity2,
                    (LV2_Atom_Event*)&msg);
        }

        self->previous_source = source;
    }

    switch ((SourceEnum)source)
    {
        case SOURCE_1_AND_2:
            self->p_port_events_in1 = self->port_events_in1;
            self->p_port_events_in2 = self->port_events_in2;
            break;
        case SOURCE_3_AND_4:
            self->p_port_events_in1 = self->port_events_in3;
            self->p_port_events_in2 = self->port_events_in4;
            break;
    }

    LV2_ATOM_SEQUENCE_FOREACH(self->p_port_events_in1, ev)
    {
        if (ev->body.type == self->urid_midiEvent)
        {
            lv2_atom_sequence_append_event(self->port_events_out1,
                                           out_capacity1,
                                           ev);
        }
    }

    LV2_ATOM_SEQUENCE_FOREACH(self->p_port_events_in2, ev)
    {
        if (ev->body.type == self->urid_midiEvent)
        {
            lv2_atom_sequence_append_event(self->port_events_out2,
                                           out_capacity2,
                                           ev);
        }
    }
}

static void cleanup(LV2_Handle instance)
{
    free(instance);
}

static const LV2_Descriptor descriptor = {
    .URI = "http://moddevices.com/plugins/mod-devel/midi-switchbox_2-1_2C",
    .instantiate = instantiate,
    .connect_port = connect_port,
    .activate = activate,
    .run = run,
    .deactivate = NULL,
    .cleanup = cleanup,
    .extension_data = NULL
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    return (index == 0) ? &descriptor : NULL;
}
