import {
  Coordinates,
  CalculationMethod,
  PrayerTimes,
  Madhab,
  HighLatitudeRule,
  PolarCircleResolution,
  SunnahTimes,
} from 'adhan';
import * as fs from 'fs';
import * as path from 'path';

interface City {
  name: string;
  lat: number;
  lng: number;
  tz: string;
}

const CITIES: City[] = [
  { name: 'Dhaka', lat: 23.8103, lng: 90.4125, tz: 'Asia/Dhaka' },
  { name: 'Karachi', lat: 24.8607, lng: 67.0011, tz: 'Asia/Karachi' },
  { name: 'Makkah', lat: 21.4225, lng: 39.8262, tz: 'Asia/Riyadh' },
  { name: 'Cairo', lat: 30.0444, lng: 31.2357, tz: 'Africa/Cairo' },
  { name: 'Istanbul', lat: 41.0082, lng: 28.9784, tz: 'Europe/Istanbul' },
  { name: 'London', lat: 51.5074, lng: -0.1278, tz: 'Europe/London' },
  { name: 'Oslo', lat: 59.9139, lng: 10.7522, tz: 'Europe/Oslo' },
  { name: 'Reykjavik', lat: 64.1466, lng: -21.9426, tz: 'Atlantic/Reykjavik' },
  { name: 'New York', lat: 40.7128, lng: -74.0060, tz: 'America/New_York' },
  { name: 'Sydney', lat: -33.8688, lng: 151.2093, tz: 'Australia/Sydney' },
  { name: 'Jakarta', lat: -6.2088, lng: 106.8456, tz: 'Asia/Jakarta' },
  { name: 'Auckland', lat: -36.8485, lng: 174.7633, tz: 'Pacific/Auckland' },
];

const DATES = [
  '2024-02-29', // Leap year
  '2024-03-20', // Vernal equinox
  '2024-03-31', // UK/Europe DST start
  '2024-06-21', // Summer solstice
  '2024-09-22', // Autumnal equinox
  '2024-10-27', // UK/Europe DST end
  '2024-12-21', // Winter solstice
];

const METHODS = [
  'MuslimWorldLeague',
  'Egyptian',
  'Karachi',
  'UmmAlQura',
  'Dubai',
  'MoonsightingCommittee',
  'NorthAmerica',
  'Kuwait',
  'Qatar',
  'Singapore',
  'Tehran',
  'Turkey',
] as const;

const HIGH_LAT_RULES = ['MiddleOfTheNight', 'SeventhOfTheNight', 'TwilightAngle'] as const;
const POLAR_RESOLUTIONS = ['Unresolved', 'AqrabYaum', 'AqrabBalad'] as const;

interface FixtureEntry {
  city: string;
  lat: number;
  lng: number;
  date: string;
  method: string;
  madhab: string;
  high_lat_rule: string;
  polar_resolution: string;
  // null when adhan-js returns an invalid Date (event does not occur)
  fajr_ms: number | null;
  sunrise_ms: number | null;
  dhuhr_ms: number | null;
  asr_ms: number | null;
  sunset_ms: number | null;
  maghrib_ms: number | null;
  isha_ms: number | null;
  midnight_ms: number | null;
  last_third_ms: number | null;
}

const ms = (d: Date): number | null => {
  const t = d.getTime();
  return Number.isFinite(t) ? t : null;
};

function computeEntry(
  name: string,
  lat: number,
  lng: number,
  dateStr: string,
  methodName: (typeof METHODS)[number],
  madhab: 'Shafi' | 'Hanafi',
  hlName: (typeof HIGH_LAT_RULES)[number],
  polarName: (typeof POLAR_RESOLUTIONS)[number],
): FixtureEntry {
  const [y, m, d] = dateStr.split('-').map(Number);
  // Construct local Date so getFullYear/getMonth/getDate inside adhan-js return exact Y, M, D
  const date = new Date(y!, m! - 1, d!, 12, 0, 0);
  const params = CalculationMethod[methodName]();
  params.madhab = madhab === 'Hanafi' ? Madhab.Hanafi : Madhab.Shafi;
  params.highLatitudeRule = HighLatitudeRule[hlName];
  params.polarCircleResolution = PolarCircleResolution[polarName];

  const pt = new PrayerTimes(new Coordinates(lat, lng), date, params);
  const sunnah = new SunnahTimes(pt);

  return {
    city: name,
    lat,
    lng,
    date: dateStr,
    method: methodName,
    madhab,
    high_lat_rule: hlName,
    polar_resolution: polarName,
    fajr_ms: ms(pt.fajr),
    sunrise_ms: ms(pt.sunrise),
    dhuhr_ms: ms(pt.dhuhr),
    asr_ms: ms(pt.asr),
    sunset_ms: ms(pt.sunset),
    maghrib_ms: ms(pt.maghrib),
    isha_ms: ms(pt.isha),
    midnight_ms: ms(sunnah.middleOfTheNight),
    last_third_ms: ms(sunnah.lastThirdOfTheNight),
  };
}

const fixtures: FixtureEntry[] = [];

// 1. City matrix (build plan unit 1.1)
for (const city of CITIES) {
  for (const dateStr of DATES) {
    for (const methodName of METHODS) {
      for (const madhab of ['Shafi', 'Hanafi'] as const) {
        const hlName = Math.abs(city.lat) > 55 ? 'SeventhOfTheNight' : 'MiddleOfTheNight';
        const polarName = Math.abs(city.lat) >= 64 ? 'AqrabYaum' : 'Unresolved';
        fixtures.push(computeEntry(city.name, city.lat, city.lng, dateStr, methodName, madhab, hlName, polarName));
      }
    }
  }
}

// 2. Seeded random sweep: every high-latitude rule and polar resolver, polar and date-line
//    longitudes, any day 2020-2030. Covers the night-fraction, rounding and polar-resolution
//    paths the city matrix barely touches. Deterministic (fixed LCG seed).
let seed = 20261111;
const rnd = (): number => (seed = (seed * 1103515245 + 12345) % 2147483648) / 2147483648;
const pick = <T>(xs: readonly T[]): T => xs[Math.floor(rnd() * xs.length)]!;
const pad = (n: number) => String(n).padStart(2, '0');
const randomFixtures: FixtureEntry[] = [];
const RANDOM_COUNT = 1500;
for (let i = 0; i < RANDOM_COUNT; i++) {
  const band = rnd();
  const sign = rnd() < 0.5 ? -1 : 1;
  const lat = band < 0.4 ? rnd() * 120 - 60 : band < 0.75 ? sign * (48 + rnd() * 20) : sign * (65 + rnd() * 24);
  const lng = rnd() * 360 - 180;
  const dateStr = `${2020 + Math.floor(rnd() * 11)}-${pad(1 + Math.floor(rnd() * 12))}-${pad(1 + Math.floor(rnd() * 28))}`;
  randomFixtures.push(
    computeEntry(`random-${i}`, Number(lat.toFixed(4)), Number(lng.toFixed(4)), dateStr, pick(METHODS),
      rnd() < 0.5 ? 'Shafi' : 'Hanafi', pick(HIGH_LAT_RULES), pick(POLAR_RESOLUTIONS)),
  );
}

// 3. Regressions: inputs that once exposed real engine bugs (2026-11-12 review). Expected values
//    come from adhan-js like every other fixture, so no hand-written numbers live in the tests.
//    Add a case here only for a real bug that the matrix and the sweep above did not catch.
const REGRESSIONS: Array<[string, number, number, string]> = [
  // Next-day Fajr for midnight/last third skipped the safe-Fajr bound (Fajr near solar midnight).
  ['regression-safe-fajr-south', -52.0, 50.8, '2027-01-20'],
  ['regression-safe-fajr-dateline', 0.0, -180.0, '2027-06-12'],
  ['regression-safe-fajr-north', 56.0, 97.6, '2027-08-08'],
];
for (const [name, lat, lng, dateStr] of REGRESSIONS) {
  randomFixtures.push(computeEntry(name, lat, lng, dateStr, 'MuslimWorldLeague', 'Shafi', 'MiddleOfTheNight', 'Unresolved'));
}

const outDir = path.resolve(__dirname, '../../native/tests/fixtures');
if (!fs.existsSync(outDir)) {
  fs.mkdirSync(outDir, { recursive: true });
}

const outFile = path.join(outDir, 'prayer_fixtures.json');
fs.writeFileSync(outFile, JSON.stringify(fixtures, null, 2), 'utf-8');
console.log(`Generated ${fixtures.length} prayer time test fixtures -> ${outFile}`);

const randomFile = path.join(outDir, 'prayer_fixtures_random.json');
fs.writeFileSync(randomFile, JSON.stringify(randomFixtures) + '\n', 'utf-8');
console.log(`Generated ${randomFixtures.length} sweep + regression prayer time test fixtures -> ${randomFile}`);

// 4. Hijri reference (build plan unit 1.7): every day of 2024-2025 from ICU's `islamic-civil`
//    calendar, the same tabular (civil epoch) algorithm the engine implements. Rows are
//    [gYear, gMonth, gDay, hYear, hMonth, hDay]; consecutive days let the test check ±2 adjustments.
const hijriFmt = new Intl.DateTimeFormat('en-u-ca-islamic-civil-nu-latn', {
  timeZone: 'UTC', year: 'numeric', month: 'numeric', day: 'numeric',
});
const hijriRows: number[][] = [];
for (let t = Date.UTC(2024, 0, 1); t <= Date.UTC(2025, 11, 31); t += 86_400_000) {
  const d = new Date(t);
  const parts = Object.fromEntries(hijriFmt.formatToParts(d).map((p) => [p.type, p.value]));
  hijriRows.push([d.getUTCFullYear(), d.getUTCMonth() + 1, d.getUTCDate(),
    parseInt(parts.year!, 10), parseInt(parts.month!, 10), parseInt(parts.day!, 10)]);
}
const hijriFile = path.join(outDir, 'hijri_fixtures.json');
fs.writeFileSync(hijriFile, JSON.stringify(hijriRows) + '\n', 'utf-8');
console.log(`Generated ${hijriRows.length} Hijri reference days -> ${hijriFile}`);
