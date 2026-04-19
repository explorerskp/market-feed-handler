import struct
import os
import sys


INPUT_FILE = "market_data.bin"
OUTPUT_FILE = "multi_data.bin"


ASSETS = [
    (0, 1.0),       
    (1, 0.035),     
    (2, 0.0015),    
    (3, 0.006),     
    (4, 0.00001)    
]



PACKET_FMT = "=Qccdd" 
PACKET_SIZE = struct.calcsize(PACKET_FMT) 

def multiplex_data():
    if not os.path.exists(INPUT_FILE):
        print(f"Error: {INPUT_FILE} not found.")
        return

    file_size = os.path.getsize(INPUT_FILE)
    print(f"Input file size: {file_size} bytes")
    print(f"Packet size: {PACKET_SIZE} bytes")
    
    total_packets = file_size // PACKET_SIZE
    print(f"Input: {total_packets} packets (BTC only)")
    print(f"Generating Multi-Asset Data (x{len(ASSETS)} volume)...")

    with open(INPUT_FILE, "rb") as fin, open(OUTPUT_FILE, "wb") as fout:
        count = 0
        
        while True:
            bytes_data = fin.read(PACKET_SIZE)
            if len(bytes_data) < PACKET_SIZE:
                break
            
            try:
                
                ts, type_char, side_char, price, qty = struct.unpack(PACKET_FMT, bytes_data)
                
                
                for symbol_id, multiplier in ASSETS:
                    
                    new_price = price * multiplier
                    
                    
                    new_ts = (ts // 5) * 5 + symbol_id
                    
                    
                    new_packet = struct.pack(PACKET_FMT, new_ts, type_char, side_char, new_price, qty)
                    fout.write(new_packet)
                
                count += 1
                if count % 100000 == 0:
                    print(f"Processed {count} input packets...", end='\r')
            except struct.error as e:
                print(f"\nStruct error at packet {count}: {e}")
                break

    print(f"\nDone! Generated {count * len(ASSETS)} packets in {OUTPUT_FILE}.")

if __name__ == "__main__":
    multiplex_data()