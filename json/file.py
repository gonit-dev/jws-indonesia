#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import requests
import ast

def parse_json_file(filename):
    """Parse JSON file without using json module"""
    with open(filename, 'r', encoding='utf-8') as f:
        content = f.read()
    return ast.literal_eval(content)

def write_json_file(filename, data):
    """Write JSON file without using json module"""
    with open(filename, 'w', encoding='utf-8') as f:
        f.write('[\n')
        for i, item in enumerate(data):
            f.write('  {\n')
            f.write('    "api": "{}",\n'.format(item['api']))
            f.write('    "display": "{}",\n'.format(item['display']))
            f.write('    "province": "{}",\n'.format(item['province']))
            if item['lat'] is not None:
                f.write('    "lat": {},\n'.format(item['lat']))
                f.write('    "lon": {}\n'.format(item['lon']))
            else:
                f.write('    "lat": null,\n')
                f.write('    "lon": null\n')
            if i < len(data) - 1:
                f.write('  },\n')
            else:
                f.write('  }\n')
        f.write(']\n')

def geocode_location(api_name, province):
    """
    Geocode a location using OpenStreetMap Nominatim API
    FIXED: Now uses api field from JSON, not display field
    """
    base_url = "https://nominatim.openstreetmap.org/search"
    
    # PENTING: Gunakan api_name langsung (sudah dari field 'api' di JSON)
    print("  Searching with API name: '{}'".format(api_name))
    
    # Strategy pencarian yang lebih fokus
    search_queries = [
        # Strategy 1: API name + province (paling akurat)
        "{}, {}".format(api_name, province),
        
        # Strategy 2: API name + Indonesia
        "{}, Indonesia".format(api_name),
        
        # Strategy 3: API name saja
        api_name,
        
        # Strategy 4: Untuk Kabupaten, coba tanpa prefix
        api_name.replace("Kabupaten ", "") if "Kabupaten" in api_name else None,
        
        # Strategy 5: Format lengkap
        "{}, {}, Indonesia".format(api_name, province),
    ]
    
    # Remove None values
    search_queries = [q for q in search_queries if q is not None]
    
    headers = {
        'User-Agent': 'Indonesia-Cities-Geocoder/1.0'
    }
    
    for idx, query in enumerate(search_queries, 1):
        print("  Strategy {}: '{}'".format(idx, query))
        
        params = {
            'q': query,
            'format': 'json',
            'limit': 3,
            'countrycodes': 'id'
        }
        
        try:
            response = requests.get(base_url, params=params, headers=headers, timeout=10)
            response.raise_for_status()
            data = response.json()
            
            if data and len(data) > 0:
                result = data[0]
                print("  -> Found: {} (type: {})".format(
                    result.get('display_name', 'N/A'), 
                    result.get('type', 'N/A')
                ))
                return {
                    'lat': float(result['lat']),
                    'lon': float(result['lon'])
                }
            else:
                print("  -> No results")
            
            time.sleep(1.5)
            
        except Exception as e:
            print("  -> Error: {}".format(str(e)))
            time.sleep(2)
    
    return {'lat': None, 'lon': None}

def load_existing_results(output_file):
    """Load existing results if available"""
    try:
        results = parse_json_file(output_file)
        print("Found existing results with {} entries".format(len(results)))
        return results
    except:
        return []

def process_cities(input_file, output_file, retry_failed=False):
    """
    Process cities.json and add coordinates
    FIXED: Uses 'api' field for geocoding
    """
    # Read input file
    try:
        original_cities = parse_json_file(input_file)
    except Exception as e:
        print("Error reading input file: {}".format(str(e)))
        return
    
    total = len(original_cities)
    
    # Load existing results
    existing_results = load_existing_results(output_file)
    
    if retry_failed and existing_results:
        # Mode retry: hanya proses yang gagal
        print("\n=== RETRY MODE ===")
        print("Will retry only failed cities from previous run")
        
        # Pisahkan yang berhasil dan gagal
        successful = [city for city in existing_results if city['lat'] is not None]
        failed_cities = [city for city in existing_results if city['lat'] is None]
        
        print("Keeping {} successful cities".format(len(successful)))
        print("Retrying {} failed cities".format(len(failed_cities)))
        print("=" * 60)
        
        results = successful
        cities_to_process = failed_cities
        start_index = 0
        
    elif existing_results:
        # Ada hasil sebelumnya, tanya mau lanjut atau tidak
        print("\nFound existing progress:")
        successful = sum(1 for r in existing_results if r['lat'] is not None)
        print("- Total: {} entries".format(len(existing_results)))
        print("- Successful: {}".format(successful))
        print("- Failed: {}".format(len(existing_results) - successful))
        
        response = raw_input("\nContinue from where it stopped? (y/n): ")
        if response.lower() == 'y':
            results = existing_results
            cities_to_process = original_cities[len(existing_results):]
            start_index = len(existing_results)
        else:
            results = []
            cities_to_process = original_cities
            start_index = 0
    else:
        # Mulai dari awal
        results = []
        cities_to_process = original_cities
        start_index = 0
    
    print("\nProcessing {} locations...".format(len(cities_to_process)))
    print("=" * 60)
    
    for i, city in enumerate(cities_to_process):
        index = start_index + i + 1
        
        print("\n[{}/{}] Processing: {}".format(index, total, city['display']))
        print("  Province: {}".format(city['province']))
        
        # PENTING: Gunakan field 'api' untuk geocoding, bukan 'display'
        coords = geocode_location(city['api'], city['province'])
        
        # Add coordinates to city data
        city_with_coords = {
            'api': city['api'],
            'display': city['display'],
            'province': city['province'],
            'lat': coords['lat'],
            'lon': coords['lon']
        }
        
        results.append(city_with_coords)
        
        if coords['lat'] is not None:
            print("  ✓ Found: lat={}, lon={}".format(coords['lat'], coords['lon']))
        else:
            print("  ✗ Not found - coordinates set to null")
        
        # Save progress every 10 items
        if (i + 1) % 10 == 0:
            try:
                write_json_file(output_file, results)
                successful_now = sum(1 for r in results if r['lat'] is not None)
                print("\n--- Progress saved ({}/{}) - Success rate: {:.1f}% ---".format(
                    len(results), total, (successful_now/len(results))*100))
            except Exception as e:
                print("Error saving progress: {}".format(str(e)))
        
        # Respect API rate limits
        time.sleep(1.5)
    
    # Final save
    try:
        write_json_file(output_file, results)
    except Exception as e:
        print("Error saving final results: {}".format(str(e)))
        return
    
    # Statistics
    successful = sum(1 for r in results if r['lat'] is not None)
    failed = len(results) - successful
    
    print("\n" + "=" * 60)
    print("GEOCODING COMPLETE!")
    print("=" * 60)
    print("Total locations: {}".format(len(results)))
    print("Successfully geocoded: {}".format(successful))
    print("Failed: {}".format(failed))
    print("Success rate: {:.1f}%".format((successful/len(results))*100))
    print("\nOutput saved to: {}".format(output_file))
    
    if failed > 0:
        print("\nTo retry only failed cities, run:")
        print("python {} --retry".format(__file__))

if __name__ == "__main__":
    import sys
    
    input_file = "cities.json"
    output_file = "cities_with_coordinates.json"
    
    retry_mode = '--retry' in sys.argv
    
    print("Indonesia Cities Geocoder - FIXED VERSION")
    print("=" * 60)
    print("Input file: {}".format(input_file))
    print("Output file: {}".format(output_file))
    print("\nFIXES:")
    print("- NOW USES 'api' FIELD from JSON (not 'display')")
    print("- More accurate search queries")
    print("- Better logging to show which field is being used")
    print("- Can resume from existing progress")
    print("- Can retry only failed cities (--retry)")
    print("=" * 60)
    
    if retry_mode:
        print("\n*** RETRY MODE: Will retry only failed cities ***\n")
    
    try:
        process_cities(input_file, output_file, retry_failed=retry_mode)
    except KeyboardInterrupt:
        print("\n\nStopped by user. Progress has been saved.")
    except Exception as e:
        print("\nError: {}".format(str(e)))
        import traceback
        traceback.print_exc()