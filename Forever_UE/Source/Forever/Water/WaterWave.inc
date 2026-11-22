// WaterWave.inc -- function body of Height(x, y, t, iterations) -> float (cm).
// Shared verbatim by C++ (WaterWave.h #includes it) and the HLSL Custom node of M_WaterSurface / PP_Underwater
// (Tools/WaterMaterial/build_water_assets.py pastes this file's text into the node). Keep to the C++/HLSL common
// subset: scalar floats, sin/cos/exp/sqrt/floor/ceil, for/if/break, no arrays, no vector types, no comments with
// non-ASCII characters.
//
// Based on "Very fast procedural ocean" by afl_ext (Shadertoy MdXyzX, MIT License): getwaves() + wavedx().
// Differences from the original: world position is scaled from cm into wave space (WAVE_SCALE), the result is
// centered and scaled to centimeters, the direction step is reduced mod 2*pi (1232.399963 mod 2*pi), and the
// number of octaves is a float so the octave count can change continuously with distance.
//
// Inputs : x, y  world position in cm;  t  time in seconds;  iterations  number of octaves (float, >= 1)
// Result : water surface height offset in cm (zero mean)

const float WAVE_SCALE = 0.0025f;       // wave space units per cm (base wavelength 2*pi/0.0025 = ~25 m)
const float AMPLITUDE = 50.0f;          // cm, scales the (centered) 0..1 wave sum
const float DRAG_MULT = 0.38f;          // how much waves pull on the position of later octaves
const float ITER_ANGLE = 0.8956427928f; // 1232.399963 mod 2*pi: direction rotation per octave (radians)
const float WAVE_MEAN = 0.46577f;       // mean of exp(sin(x) - 1), centers the sum around zero

float n = iterations;
if (n < 1.0f) n = 1.0f;
float count = ceil(n);

float px = x * WAVE_SCALE;
float py = y * WAVE_SCALE;
float phaseShift = sqrt(px * px + py * py) * 0.1f; // avoids every octave having the same phase everywhere

float iter = 0.0f;
float frequency = 1.0f;
float timeMultiplier = 2.0f;
float weight = 1.0f;
float sumValues = 0.0f;
float sumWeights = 0.0f;

for (int i = 0; i < 48; i++) {
	if (float(i) >= count) break;

	// fractional octave count: the last octave fades in as iterations grows
	float k = n - float(i);
	if (k > 1.0f) k = 1.0f;

	float dirx = sin(iter);
	float diry = cos(iter);

	float xx = (dirx * px + diry * py) * frequency + t * timeMultiplier + phaseShift;
	float wave = exp(sin(xx) - 1.0f);
	float dx = wave * cos(xx);

	float we = weight * k;
	px += dirx * (-dx) * we * DRAG_MULT;
	py += diry * (-dx) * we * DRAG_MULT;

	sumValues += wave * we;
	sumWeights += we;

	weight = weight * 0.8f; // mix(weight, 0.0, 0.2)
	frequency = frequency * 1.18f;
	timeMultiplier = timeMultiplier * 1.07f;
	iter += ITER_ANGLE;
}

return 0.0f; // flat water for now (waves disabled): restore the line below to re-enable
// return AMPLITUDE * (sumValues / sumWeights - WAVE_MEAN);
