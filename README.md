# MAL （Media Analysis Tool ）
媒体分析工具
## 支持格式
### FMP4/MP4/HEIF/MOV
AVCC解析，包括SPS/PPS <br/>
HVCC解析，包括VPS/SPS/PPS <br/>
包括以下box解析<br/>
- [x] ftyp
- [x] mvhd
- [x] tkhd
- [x] moov|trak|tref|hint|font|vdep|vplx|subt|trgr|msrc|mdia|minf|udta|edts|iprp|ipco|moof|traf|mvex|hoov
- [x] meta
- [x] mdhd
- [x] hdlr
- [x] elng
- [x] stbl
- [x] stsd
- [x] mp4a|ipcm|fpcm
- [x] avc1|avc2
- [x] avcC
- [x] elst
- [x] stts
- [x] stss
- [x] ctts
- [x] stsh
- [x] stco
- [x] co64
- [x] stsc
- [x] stsz
- [x] pitm
- [x] iloc
- [x] iinf
- [x] infe
- [x] iref
- [x] thmb|dimg|cdsc|auxl
- [x] ipma
- [x] sidx
- [x] tfhd
- [x] trun
- [x] trex
- [x] tfdt
- [x] mfhd
- [x] hvc1|hev1
- [x] hvcC
- [x] ispe
- [x] mfhd
      
<details>
  <summary>MP4 Box Dump JSON</summary>
  <pre><code>
    [{
		"root":	{
			"size":	87944366,
			"pos":	0,
			"children":	[{
					"ftyp":	{
						"size":	32,
						"pos":	0,
						"major_brand(32bits)":	"isom",
						"minor_version(32bits)":	512,
						"compatible_brands(128bits)":	"isomiso2avc1mp41"
					}
				}, {
					"free":	{
						"size":	8,
						"pos":	32
					}
				}, {
					"mdat":	{
						"size":	87344246,
						"pos":	40
					}
				}, {
					"moov":	{
						"size":	600080,
						"pos":	87344286,
						"children":	[{
								"mvhd":	{
									"size":	108,
									"pos":	8,
									"version(8bits)":	0,
									"flags(24bits)":	0,
									"creation_time(32bits)":	0,
									"modification_time(32bits)":	0,
									"timescale(32bits)":	1000,
									"duration(32bits)":	629656,
									"rate(32bits)":	65536,
									"volume(16bits)":	256,
									"reserved(16bits)":	0,
									"reserved(64bits)":	0,
									"matrix(288bits)":	"0x00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 40 00 00 00 ",
									"pre_defined(192bits)":	"0x00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ",
									"next_track_ID(32bits)":	3
								}
							}, {
								"trak":	{
									"size":	382364,
									"pos":	116,
									"children":	[{
											"tkhd":	{
												"size":	92,
												"pos":	8,
												"version(8bits)":	0,
												"flags(24bits)":	3,
												"creation_time(32bits)":	0,
												"modification_time(32bits)":	0,
												"track_ID(32bits)":	1,
												"reserved1(32bits)":	0,
												"duration(32bits)":	629656,
												"reserved2(64bits)":	0,
												"layer(16bits)":	0,
												"alternate_group(16bits)":	1,
												"volume(16bits)":	256,
												"reserved3(16bits)":	0,
												"matrix(288bits)":	"0x00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 40 00 00 00 ",
												"width(32bits)":	0,
												"height(32bits)":	0
											}
										}, {
											"edts":	{
												"size":	36,
												"pos":	100
											}
										}, {
											"mdia":	{
												"size":	382228,
												"pos":	136,
												"children":	[{
														"mdhd":	{
															"size":	32,
															"pos":	8,
															"version(8bits)":	0,
															"flags(24bits)":	0,
															"creation_time(32bits)":	0,
															"modification_time(32bits)":	0,
															"timescale(32bits)":	44100,
															"duration(32bits)":	27767808,
															"pad_language(16bits)":	21956,
															"pre_defined(16bits)":	0
														}
													}, {
														"hdlr":	{
															"size":	45,
															"pos":	40,
															"version(8bits)":	0,
															"flags(24bits)":	0,
															"pre_defined(32bits)":	0,
															"handler_type(32bits)":	"soun",
															"reserved(96bits)":	"0x00 00 00 00 00 00 00 00 00 00 00 00 ",
															"name(104bits)":	"SoundHandler"
														}
													}, {
														"minf":	{
															"size":	382143,
															"pos":	85,
															"children":	[{
																	"smhd":	{
																		"size":	16,
																		"pos":	8
																	}
																}, {
																	"dinf":	{
																		"size":	36,
																		"pos":	24
																	}
																}, {
																	"stbl":	{
																		"size":	382083,
																		"pos":	60,
																		"children":	[{
																				"stsd":	{
																					"size":	117,
																					"pos":	8,
																					"version(8bits)":	0,
																					"flags(24bits)":	0,
																					"children":	[{
																							"mp4a":	{
																								"size":	101,
																								"pos":	16,
																								"reserved(48bits)":	0,
																								"data_reference_index(16bits)":	1,
																								"version(16bits)":	0,
																								"reserved(48bits)":	0,
																								"channelcount(16bits)":	2,
																								"samplesize(16bits)":	16,
																								"pre_defined(16bits)":	0,
																								"reserved(16bits)":	0,
																								"samplerate(32bits)":	44100,
																								"children":	[{
																										"esds":	{
																											"size":	65,
																											"pos":	36
																										}
																									}]
																							}
																						}]
																				}
																			}, {
																				"stts":	{
																					"size":	24,
																					"pos":	125
																				}
																			}, {
																				"stsc":	{
																					"size":	197896,
																					"pos":	149
																				}
																			}, {
																				"stsz":	{
																					"size":	108488,
																					"pos":	198045
																				}
																			}, {
																				"stco":	{
																					"size":	75496,
																					"pos":	306533
																				}
																			}, {
																				"sgpd":	{
																					"size":	26,
																					"pos":	382029
																				}
																			}, {
																				"sbgp":	{
																					"size":	28,
																					"pos":	382055
																				}
																			}]
																	}
																}]
														}
													}]
											}
										}]
								}
							}, {
								"trak":	{
									"size":	217301,
									"pos":	382480,
									"children":	[{
											"tkhd":	{
												"size":	92,
												"pos":	8,
												"version(8bits)":	0,
												"flags(24bits)":	3,
												"creation_time(32bits)":	0,
												"modification_time(32bits)":	0,
												"track_ID(32bits)":	2,
												"reserved1(32bits)":	0,
												"duration(32bits)":	629596,
												"reserved2(64bits)":	0,
												"layer(16bits)":	0,
												"alternate_group(16bits)":	0,
												"volume(16bits)":	0,
												"reserved3(16bits)":	0,
												"matrix(288bits)":	"0x00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 40 00 00 00 ",
												"width(32bits)":	1280,
												"height(32bits)":	720
											}
										}, {
											"edts":	{
												"size":	48,
												"pos":	100
											}
										}, {
											"mdia":	{
												"size":	217153,
												"pos":	148,
												"children":	[{
														"mdhd":	{
															"size":	32,
															"pos":	8,
															"version(8bits)":	0,
															"flags(24bits)":	0,
															"creation_time(32bits)":	0,
															"modification_time(32bits)":	0,
															"timescale(32bits)":	90000,
															"duration(32bits)":	56663607,
															"pad_language(16bits)":	21956,
															"pre_defined(16bits)":	0
														}
													}, {
														"hdlr":	{
															"size":	45,
															"pos":	40,
															"version(8bits)":	0,
															"flags(24bits)":	0,
															"pre_defined(32bits)":	0,
															"handler_type(32bits)":	"vide",
															"reserved(96bits)":	"0x00 00 00 00 00 00 00 00 00 00 00 00 ",
															"name(104bits)":	"VideoHandler"
														}
													}, {
														"minf":	{
															"size":	217068,
															"pos":	85,
															"children":	[{
																	"vmhd":	{
																		"size":	20,
																		"pos":	8
																	}
																}, {
																	"dinf":	{
																		"size":	36,
																		"pos":	28
																	}
																}, {
																	"stbl":	{
																		"size":	217004,
																		"pos":	64,
																		"children":	[{
																				"stsd":	{
																					"size":	168,
																					"pos":	8,
																					"version(8bits)":	0,
																					"flags(24bits)":	0,
																					"children":	[{
																							"avc1":	{
																								"size":	152,
																								"pos":	16,
																								"reserved(48bits)":	0,
																								"data_reference_index(16bits)":	1,
																								"pre_defined(16bits)":	0,
																								"reserved(16bits)":	0,
																								"pre_defined(96bits)":	"0x00 00 00 00 00 00 00 00 00 00 00 00 ",
																								"width(16bits)":	1280,
																								"height(16bits)":	720,
																								"horizresolution(32bits)":	4718592,
																								"vertresolution(32bits)":	4718592,
																								"reserved(32bits)":	0,
																								"frame_count(16bits)":	1,
																								"compressorname(256bits)":	"",
																								"depth(16bits)":	24,
																								"pre_defined(16bits)":	65535,
																								"children":	[{
																										"avcC":	{
																											"size":	50,
																											"pos":	86
																										}
																									}, {
																										"pasp":	{
																											"size":	16,
																											"pos":	136
																										}
																									}]
																							}
																						}]
																				}
																			}, {
																				"stts":	{
																					"size":	24,
																					"pos":	176
																				}
																			}, {
																				"stss":	{
																					"size":	620,
																					"pos":	200
																				}
																			}, {
																				"ctts":	{
																					"size":	65168,
																					"pos":	820
																				}
																			}, {
																				"stsc":	{
																					"size":	28,
																					"pos":	65988
																				}
																			}, {
																				"stsz":	{
																					"size":	75496,
																					"pos":	66016
																				}
																			}, {
																				"stco":	{
																					"size":	75492,
																					"pos":	141512
																				}
																			}]
																	}
																}]
														}
													}]
											}
										}]
								}
							}, {
								"udta":	{
									"size":	299,
									"pos":	599781
								}
							}]
					}
				}]
		}
	}]
  </code></pre>
</details>

### FLV

- [x] audio tag
- [x] video tag (支持enhanced flv)
- [x] script tag
### WEBP
