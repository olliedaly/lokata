import serial
import time
import csv
import sys
import glob
import os

# --- CONFIGURATION ---
BAUD_RATE = 921600       # Must match Arduino
SERIAL_PORT = None       # Set to "COMX" or "/dev/ttyUSB0" if auto-detect fails
OUTPUT_DIR = os.path.join("data", "raw") # Relative path for data

def get_serial_ports():
    """ Lists serial port names """
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Unsupported platform')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
    return result

def main():
    # 1. Output Directory Setup
    abs_output_dir = os.path.abspath(OUTPUT_DIR)
    if not os.path.exists(abs_output_dir):
        os.makedirs(abs_output_dir)
    
    # 2. Port Selection
    port = SERIAL_PORT
    if port is None:
        print("🔍 Scanning ports...")
        available_ports = get_serial_ports()
        if not available_ports:
            print("❌ No ports found.")
            return
        if len(available_ports) == 1:
            port = available_ports[0]
            print(f"✅ Auto-selected: {port}")
        else:
            for i, p in enumerate(available_ports):
                print(f"  {i}: {p}")
            idx = int(input("Select index: "))
            port = available_ports[idx]

    # 3. Serial Connection
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=1)
        ser.reset_input_buffer()
        print(f"🔌 Connected to {port} @ {BAUD_RATE}")
    except Exception as e:
        print(f"❌ Connection Failed: {e}")
        return

    # 4. File Setup
    timestamp = time.strftime("%Y%m%d-%H%M%S")
    filename = f"walk_data_{timestamp}.csv"
    full_path = os.path.join(abs_output_dir, filename)
    
    header = ["Millis", "Sats", "Lat", "Lon", "HDOP", 
              "Ax", "Ay", "Az", "Gx", "Gy", "Gz", "Mx", "My", "Mz"]

    print(f"\n📝 Logging to: {full_path}")
    print("❌ Press Ctrl+C to stop.\n")

    with open(full_path, mode='w', newline='') as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(header)
        
        row_count = 0
        start_time = time.time()

        try:
            while True:
                if ser.in_waiting > 0:
                    try:
                        line = ser.readline().decode('utf-8', errors='ignore').strip()
                        
                        if line.startswith("DATA,"):
                            parts = line.split(',')
                            if len(parts) >= 15: # Validation
                                data_row = parts[1:] # Skip "DATA" tag
                                writer.writerow(data_row)
                                row_count += 1
                                
                                # OPTIMIZATION: Flush to disk only once per second (approx 100 rows)
                                # This prevents disk I/O from blocking the serial buffer
                                if row_count % 100 == 0:
                                    csv_file.flush()
                                    duration = time.time() - start_time
                                    hz = row_count / duration
                                    print(f"\r💾 Rows: {row_count} | Rate: {hz:.1f} Hz | Last: {line[:40]}...", end="")
                            
                    except UnicodeDecodeError:
                        pass # Ignore weird bytes at startup
                        
        except KeyboardInterrupt:
            print(f"\n\n✅ Stopped. Saved {row_count} rows.")
            ser.close()

if __name__ == "__main__":
    main()