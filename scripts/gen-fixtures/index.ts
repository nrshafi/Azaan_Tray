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

interface FixtureEntry {
  city: string;
  lat: number;
  lng: number;
  date: string;
  method: string;
  madhab: string;
  high_lat_rule: string;
  fajr_ms: number;
  sunrise_ms: number;
  dhuhr_ms: number;
  asr_ms: number;
  sunset_ms: number;
  maghrib_ms: number;
  isha_ms: number;
  midnight_ms: number;
  last_third_ms: number;
}

const fixtures: FixtureEntry[] = [];

for (const city of CITIES) {
  const coords = new Coordinates(city.lat, city.lng);

  for (const dateStr of DATES) {
    const [y, m, d] = dateStr.split('-').map(Number);
    // Construct local Date so getFullYear/getMonth/getDate inside adhan-js return exact Y, M, D
    const date = new Date(y!, m! - 1, d!, 12, 0, 0);

    for (const methodName of METHODS) {
      for (const madhab of ['Shafi', 'Hanafi'] as const) {
        const methodFn = CalculationMethod[methodName];
        const params = methodFn();
        params.madhab = madhab === 'Hanafi' ? Madhab.Hanafi : Madhab.Shafi;

        let hlName = 'MiddleOfTheNight';
        if (Math.abs(city.lat) > 55) {
          params.highLatitudeRule = HighLatitudeRule.SeventhOfTheNight;
          hlName = 'SeventhOfTheNight';
        }
        if (Math.abs(city.lat) >= 64) {
          params.polarCircleResolution = PolarCircleResolution.AqrabYaum;
        }

        const pt = new PrayerTimes(coords, date, params);
        const sunnah = new SunnahTimes(pt);

        fixtures.push({
          city: city.name,
          lat: city.lat,
          lng: city.lng,
          date: dateStr,
          method: methodName,
          madhab,
          high_lat_rule: hlName,
          fajr_ms: pt.fajr.getTime(),
          sunrise_ms: pt.sunrise.getTime(),
          dhuhr_ms: pt.dhuhr.getTime(),
          asr_ms: pt.asr.getTime(),
          sunset_ms: pt.sunset.getTime(),
          maghrib_ms: pt.maghrib.getTime(),
          isha_ms: pt.isha.getTime(),
          midnight_ms: sunnah.middleOfTheNight.getTime(),
          last_third_ms: sunnah.lastThirdOfTheNight.getTime(),
        });
      }
    }
  }
}

const outDir = path.resolve(__dirname, '../../native/tests/fixtures');
if (!fs.existsSync(outDir)) {
  fs.mkdirSync(outDir, { recursive: true });
}

const outFile = path.join(outDir, 'prayer_fixtures.json');
fs.writeFileSync(outFile, JSON.stringify(fixtures, null, 2), 'utf-8');

console.log(`Generated ${fixtures.length} prayer time test fixtures -> ${outFile}`);
