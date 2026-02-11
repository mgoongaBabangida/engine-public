#pragma once

// Conventions:
// - *_frR   = fraction of hex radius R → effective = (param * R)
// - *_overR = per hex radius (1/R)     → effective = (param / R)
// 
// ---------------- Tunables grouped into structs ----------------
//----------------------------------------------------------------------------
struct MountainParams
{
	// Envelope / footprint
	float SHOULDER_START = 0.25f; // Where fade-out starts (0..1 along radius).
                                         // ↑ later start ⇒ bigger core (risk "bucket");
                                         // ↓ earlier start ⇒ softer, wider shoulder.
	float SHARPNESS = 1.5f;  // Falloff steepness.
                           // ↑ crisper shoulder/peak; ↓ rounder mound.

	float ROUND_K = 0.95f;        // 0=hex, 1=circle
	float CIRCLE_RADIUS_FR = 0.55f; // // Circle radius as fraction of R.
                                  // ↑ wider circular base; ↓ tighter base.

	// Subtle basement jitter
	float JITTER_AMP_MIN = 0.04f;  // Min boundary wobble amplitude.
	float JITTER_AMP_MAX = 0.08f;  // Max boundary wobble amplitude.

	float LOBES_MIN = 2.0f; // Min number of scallops/lobes.
	float LOBES_MAX = 4.0f; // Max number of scallops/lobes.
                                         // ↑ more/smaller lobes (busier);
                                         // ↓ fewer/broader lobes (calmer).

	// Low-band shaping
	float LIFT_BORDER = 0.50f; // makes cone of mountain
	float LIFT_CENTER = 0.05f; // lifts cracks
	float MARGIN = 0.01f;

	// ---------------- Low-band shaping (macro lift) ----------------
	// Domain warp + detail bands
	float WARP_AMP_FRR = 0.03f; // Warp amplitude(in world units, *R).
																// ↑ curvier ridge flow (too high distorts);
																// ↓ straighter patterns.
	float WARP_FREQ_OVER_R = 0.75f;         // Mid-scale detail frequency (per R).
                                         // ↑ smaller mid features; ↓ broader undulations.

	float MESO_FREQ_OVER_R = 10.f;         // / R
	int   MESO_OCTAVES = 3; // Mid-scale octave count.
                          // ↑ richer mid detail (slower); ↓ simpler.
	float MESO_AMP = 0.165f;

	float MICRO_FREQ_OVER_R = 1.3f;         // / R
	int   MICRO_OCTAVES = 5;
	float MICRO_AMP = 0.225f;
	float LACUNARITY = 0.8f;// Frequency multiplier per octave.
                                         // ↑ faster frequency growth (more fractal);
                                         // ↓ tighter spacing (smoother cascade).
	float GAIN = 1.0f; // Amplitude falloff per octave.
                                         // ↑ keeps more high-octave energy (noisier);
                                         // ↓ damps highs (cleaner).

	// Apex
	float SPIKE_POWER = 4.0f; // Concentration of pointiness.
                                         // ↑ narrow, pointy peaks; ↓ broad apex.
	float SPIKE_AMP = 0.60f; // Spike height contribution.
                                         // ↑ taller peaks (watch margin); ↓ flatter tops.

	// High-band gain
	float HIGH_GAIN_CENTER = 1.45f; // Crack boost at center.
                                         // ↑ more high-detail in core; ↓ smoother apex.
	float HIGH_GAIN_BORDER = 0.65f; // Crack boost on slopes/edges.
                                         // ↑ more edge crackiness; ↓ cleaner sides.
	float FOOTPRINT_FR = 1.33f; // how much of the hex occupy

  float BASE_COMP_GAMMA = 1.0f; // >1 compresses highs in cores (more headroom)
  float BASE_COMP_ENV_START = 0.35f;
  float BASE_COMP_ENV_END = 0.85f;
};

//----------------------------------------------------
struct HillParams
{
  // Size & edge blend (existing)
  float H_HEIGHT_SCALE = 0.65f;
  float H_BLEND_START = 0.82f;
  float H_BLEND_END = 0.95f;

  // Dome envelope (existing)
  float H_HILL_RADIUS_FR = 0.78f;
  float H_SHOULDER_START = 0.075f;
  float H_SHARPNESS = 0.4f;
  float H_MARGIN = 0.03f;

  // Lift & top shaping (existing)
  float H_LIFT_CENTER_BASE = 0.08f;
  float H_LIFT_CENTER_GAIN = 0.65f;
  float H_TOP_SUPPRESS = 0.085f;
  float H_DOME_AMP = 0.20f;
  float H_DOME_POW = 0.50f;

  // Mid-slope taper (existing)
  float taper_exp = 1.0f;

  // Off-center apex (already working for you)
  float H_PEAK_OFFSET_MIN_FR = 0.0f;
  float H_PEAK_OFFSET_MAX_FR = 0.0f;
  float H_PEAK_POW = 1.0f;

  // soft "pre-blend" belt just INSIDE the basement edge.
  // Start it a bit earlier than H_BLEND_START and bleed in some base.
  float H_BASEMENT_PREBLEND_START = 0.60f; // default = H_BLEND_START => no change
  float H_BASEMENT_PREBLEND_STRENGTH = 0.5f;  // 0 = off (back-compat)
  float H_BASEMENT_PREBLEND_EXP = 1.8f;  // curve control (>=1). 1 = linear

  float BASE_COMP_GAMMA = 1.0f; // >1 compresses highs in cores (more headroom)
  float BASE_COMP_ENV_START = 0.35f;
  float BASE_COMP_ENV_END = 0.85f;
};

//----------------------------------------------------
struct PlainParams
{
  // Center-only, gentle normalization of plains so they don’t look “cut” when base is bright.
  // 0 strength keeps current look. Gamma > 1 compresses highs a bit.
  float REMAP_GAMMA = 1.0f;  // >1 compress highs, <1 lifts lows
  float REMAP_STRENGTH = 0.0f;  // 0..1 amount of remap in interior

  // Where the effect applies inside the hex (like a plain envelope)
  float ENV_START = 0.78f; // start of interior zone (0..1 along hex radius)
  float SHARPNESS = 1.0f;  // envelope sharpness

  // Optional “calm the tiny bumps” on plains
  float MICRO_SUPPRESS = 0.0f;  // 0..1 reduce high band in interior
};

//--------------------------------------------------------------------------
struct WaterParams
{
  // ---------------- Height & depth ----------------
  float SEA_LEVEL = 0.25f; // Target flattened level for oceans
  float DEPTH_STRENGTH = 0.80f; // 0 = keep full noise, 1 = fully flatten to SEA_LEVEL

  // ---------------- Shore band ----------------
  float SHORE_HALF_WIDTH_FR = 0.45f; // Half-width of transition band as fraction of hex R
  float SHARPNESS = 2.0f;   // >1 = sharper transition land→sea; <1 = softer

  // ---------------- Waves ----------------
  float WAVE_AMP = 0.0f;  // Amplitude of waves (0 = disabled)
  float WAVE_FREQ_OVER_R = 2.0f;  // How many wave cycles roughly per hex radius
  float WAVE_PHASE_SKEW = 1.37f; // Skew factor for mixing x/y in wave phase

  //// ---------------- 1-land "corner" hexes ----------------
  float CORNER_INNER_R_FR = 1.0f;  // inner radius from land center (R * this)
  float CORNER_OUTER_R_EXTRA = 1.0f;  // extra band beyond R: outerR = R + SHORE_HALF_WIDTH_FR * this
  float CORNER_ALIGN_COS_MAX = 0.8660254f; // cos(30°) – how far ± from 2-land edge we still align

  // Wedge fill toward 3-land neighbor ("cake slice" fix) in 1-land hexes
  float CORNER_SEAM_RIM_MIN_FR = 0.80f; // start blending near rim at this * R
  float CORNER_SEAM_COS_START = 0.60f; // minimum cos to be in the 3-land seam wedge
  float CORNER_SEAM_STRENGTH = 1.0f;  // 0–1: how strongly to pull that wedge toward filled shape

  // ---------------- 3-land "bay" hexes ----------------
  float BAY_DIR_EXP = 2.0f; // exponent on angular weighting between edges (bigger = more local)
  float BAY_ANG_EXP = 1.5f; // exponent on angular bay mask (bigger = deeper bay interior)
  float BAY_CENTER_RADIUS_FR = 0.30f; // radius from center (in R) where we flatten toward open sea

  // ---------------- 3-land ↔ 1-land harmonization ----------------
  float BAY_CORNER_COS_START = 0.65f; // min cos dir to be considered part of 1-land seam wedge
  float BAY_CORNER_CORE_R_FR = 0.20f; // inner core around hex center where we fade out the override
  float BAY_CORNER_SEAM_STRENGTH = 1.0f; // how strongly to drag that wedge toward radial corner profile
};



