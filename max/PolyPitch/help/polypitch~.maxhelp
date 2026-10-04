{
	"patcher": {
		"fileversion": 1,
		"appversion": {
			"major": 9,
			"minor": 2,
			"revision": 0,
			"architecture": "x64",
			"modernui": 1
		},
		"classnamespace": "box",
		"rect": [
			100,
			100,
			780,
			560
		],
		"boxes": [
			{
				"box": {
					"id": "t",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						20,
						15,
						300,
						30
					],
					"text": "polypitch~",
					"fontsize": 20.0,
					"fontface": 1
				}
			},
			{
				"box": {
					"id": "d",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						20,
						50,
						520,
						48
					],
					"text": "Polyphonic pitch shifter for live playing: chords stay in tune and clean, attacks come out about 2 ms late shifting down and 7 ms shifting up. The shifted sound is mono (the inputs summed); the dry signal passes in stereo and is never delayed."
				}
			},
			{
				"box": {
					"id": "adc",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						20,
						130,
						60,
						22
					],
					"outlettype": [
						"signal",
						"signal"
					],
					"text": "adc~ 1 2"
				}
			},
			{
				"box": {
					"id": "c_in",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						90,
						130,
						200,
						20
					],
					"text": "your instrument (or any signal)"
				}
			},
			{
				"box": {
					"id": "pp",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 2,
					"patching_rect": [
						20,
						260,
						150,
						22
					],
					"outlettype": [
						"signal",
						"signal"
					],
					"text": "polypitch~ -12"
				}
			},
			{
				"box": {
					"id": "a_semi",
					"maxclass": "attrui",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						200,
						170,
						200,
						22
					],
					"outlettype": [
						""
					],
					"attr": "semitones"
				}
			},
			{
				"box": {
					"id": "a_mix",
					"maxclass": "attrui",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						200,
						196,
						200,
						22
					],
					"outlettype": [
						""
					],
					"attr": "mix"
				}
			},
			{
				"box": {
					"id": "a_tone",
					"maxclass": "attrui",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						200,
						222,
						200,
						22
					],
					"outlettype": [
						""
					],
					"attr": "tone"
				}
			},
			{
				"box": {
					"id": "c_semi",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						410,
						170,
						280,
						20
					],
					"text": "-12 .. 12 semitones (also the first argument)"
				}
			},
			{
				"box": {
					"id": "c_mix",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						410,
						196,
						330,
						20
					],
					"text": "0 = dry only, 50 = both at full level, 100 = shifted only"
				}
			},
			{
				"box": {
					"id": "c_tone",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						410,
						222,
						330,
						20
					],
					"text": "how much of the tone curve for this interval is applied"
				}
			},
			{
				"box": {
					"id": "m_clear",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						200,
						248,
						45,
						22
					],
					"outlettype": [
						""
					],
					"text": "clear"
				}
			},
			{
				"box": {
					"id": "c_clear",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						250,
						248,
						200,
						20
					],
					"text": "forget everything heard so far"
				}
			},
			{
				"box": {
					"id": "g",
					"maxclass": "gain~",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						20,
						300,
						22,
						100
					],
					"outlettype": [
						"signal",
						""
					],
					"multichannelvariant": 0,
					"parameter_enable": 0
				}
			},
			{
				"box": {
					"id": "g2",
					"maxclass": "gain~",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						150,
						300,
						22,
						100
					],
					"outlettype": [
						"signal",
						""
					],
					"multichannelvariant": 0,
					"parameter_enable": 0
				}
			},
			{
				"box": {
					"id": "dac",
					"maxclass": "ezdac~",
					"numinlets": 2,
					"numoutlets": 0,
					"patching_rect": [
						20,
						420,
						45,
						45
					]
				}
			},
			{
				"box": {
					"id": "c_dac",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						75,
						432,
						100,
						20
					],
					"text": "start audio"
				}
			},
			{
				"box": {
					"id": "c_more",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						20,
						480,
						620,
						34
					],
					"text": "Latency: none is reported and the dry signal is not delayed. Sample rates: 44.1 and 48 kHz use the designed band filters; other rates fall back to a simpler, leakier filter."
				}
			}
		],
		"lines": [
			{
				"patchline": {
					"source": [
						"adc",
						0
					],
					"destination": [
						"pp",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"adc",
						1
					],
					"destination": [
						"pp",
						1
					]
				}
			},
			{
				"patchline": {
					"source": [
						"pp",
						0
					],
					"destination": [
						"g",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"pp",
						1
					],
					"destination": [
						"g2",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"g",
						0
					],
					"destination": [
						"dac",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"g2",
						0
					],
					"destination": [
						"dac",
						1
					]
				}
			},
			{
				"patchline": {
					"source": [
						"g",
						1
					],
					"destination": [
						"g2",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"a_semi",
						0
					],
					"destination": [
						"pp",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"a_mix",
						0
					],
					"destination": [
						"pp",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"a_tone",
						0
					],
					"destination": [
						"pp",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"m_clear",
						0
					],
					"destination": [
						"pp",
						0
					]
				}
			}
		]
	}
}