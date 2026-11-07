import { Coordinates, CalculationMethod, PrayerTimes, Madhab, HighLatitudeRule } from 'adhan';
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
];

interface FixtureEntry {
  city: string;
  lat: number;
  lng: number;
  date: string;
  method: string;
  madhab: string;
  fajr_utc_min: number;
  sunrise_utc_min: number;
  dhuhr_utc_min: number;
  asr_utc_min: number;
  sunset_utc_min: number;
  maghrib_utc_min: number;
  isha_utc_min: number;
}

function dateToUtcMinutes(date: Date | null): number {
  if (!date || isNaN(date.getTime())) return -1;
  return date.getUTCHours() * 60 + date.getUTCMinutes() + date.getUTCSeconds() / 60;
}

const fixtures: FixtureEntry[] = [];

for (const city of CITIES) {
  const coords = new Coordinates(city.lat, city.lng);

  for (const dateStr of DATES) {
    const [y, m, d] = dateStr.split('-').map(Number);
    // Create Date at noon UTC for stability
    const date = new Date(Date.UTC(y, m - 1, d, 12, 0, 0));

    for (const methodName of METHODS) {
      for (const madhab of ['Shafi', 'Hanafi']) {
        const methodFn = (CalculationMethod as any)[methodName];
        if (!methodFn) continue;

        const params = methodFn();
        params.madhab = madhab === 'Hanafi' ? Madhab.Hanafi : Madhab.Shafi;
        if (city.lat > 55) {
          params.highLatitudeRule = HighLatitudeRule.SeventhOfTheNight;
        }

        try {
          const pt = new PrayerTimes(coords, date, params);

          fixtures.push({
            city: city.name,
            lat: city.lat,
            lng: city.lng,
            date: dateStr,
            method: methodName,
            madhab,
            fajr_utc_min: Math.round(dateToUtcMinutes(pt.fajr) * 100) / 100,
            sunrise_utc_min: Math.round(dateToUtcMinutes(pt.sunrise) * 100) / 100,
            dhuhr_utc_min: Math.round(dateToUtcMinutes(pt.dhuhr) * 100) / 100,
            asr_utc_min: Math.round(dateToUtcMinutes(pt.asr) * 100) / 100,
            sunset_utc_min: Math.round(dateToUtcMinutes(pt.sunset) * 100) / 100,
            maghrib_utc_min: Math.round(dateToUtcMinutes(pt.maghrib) * 100) / 100,
            isha_utc_min: Math.round(dateToUtcMinutes(pt.isha) * 100) / 100,
          });
        } catch (e) {
          // Polar extreme cases without solution
        }
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
