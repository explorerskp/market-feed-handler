import pandas as pd
import struct
import sys
import time

def convert_official_csv_fast(input_csv, output_bin):
    print(f"Reading {input_csv} (Turbo Mode)...")
    
    chunk_size = 1_000_000
    total_count = 0
    start_time = time.time()
    
    
    
    
    
    
    
    struct_fmt = struct.Struct('=Qccdd')
    pack = struct_fmt.pack 

    
    TYPE_T = b'T'
    SIDE_S = b'S'
    SIDE_B = b'B'

    with open(output_bin, 'wb') as f_out:
        
        for chunk in pd.read_csv(input_csv, header=None, chunksize=chunk_size):
            
            
            
            
            
            data_buffer = bytearray()
            
            for row in chunk.itertuples(index=False, name=None):
                try:
                    
                    side = SIDE_S if row[6] else SIDE_B
                    
                    
                    data_buffer.extend(pack(
                        int(row[5]),  
                        TYPE_T,       
                        side,         
                        float(row[1]), 
                        float(row[2])  
                    ))
                    
                except Exception:
                    continue
            
            
            f_out.write(data_buffer)
            
            total_count += len(chunk)
            elapsed = time.time() - start_time
            print(f"Processed {total_count} records... ({total_count/elapsed:.0f} rows/sec)", end='\r')

    print(f"\n\nDONE! Converted {total_count} records to {output_bin}")
    print(f"Total Time: {time.time() - start_time:.2f} seconds")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python convert_fast.py <input_csv> <output_bin>")
    else:
        convert_official_csv_fast(sys.argv[1], sys.argv[2])