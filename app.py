from flask import Flask, jsonify, render_template
from bson import ObjectId
from flask_socketio import SocketIO, emit
import paho.mqtt.client as mqtt
from pymongo import MongoClient
from datetime import datetime
import json

# Konfigurasi aplikasi Flask dan SocketIO
app = Flask(__name__)
socketio = SocketIO(app)
app.config['JSONIFY_PRETTYPRINT_REGULAR'] = True  # Konfigurasi JSON pretty print

# Konfigurasi MongoDB
mongo_client = MongoClient("mongodb://localhost:27017/")
db = mongo_client["sensor_data_db"]
collection = db["sensor_data"]

# Konfigurasi MQTT
mqtt_broker = "192.168.45.116"  # IP lokal broker Mosquitto
mqtt_port = 1885           # Port broker
mqtt_topic = "sensor/data"  # Topik data sensor

# Callback MQTT saat koneksi berhasil
def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker with code: " + str(rc))
    client.subscribe(mqtt_topic)

def on_message(client, userdata, msg):
    # Terima dan parsing data JSON dari topik
    data = json.loads(msg.payload.decode())
    data["timestamp"] = datetime.now().isoformat()  # Konversi datetime ke string ISO 8601

    # Simpan data ke MongoDB
    result = collection.insert_one(data)

    # Konversi ObjectId ke string setelah disimpan ke MongoDB
    data["_id"] = str(result.inserted_id)

    # Kirim data ke klien web secara real-time
    socketio.emit("new_data", data)



# MQTT Client Setup
mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(mqtt_broker, mqtt_port, 60)
mqtt_client.loop_start()

# Route untuk halaman web
@app.route("/")
def index():
    return render_template("index.html")

def json_default(obj):
    if isinstance(obj, ObjectId):
        return str(obj)
    raise TypeError("Type not serializable")

# Fungsi untuk mengonversi string timestamp ke datetime (jika perlu)
def convert_to_datetime(timestamp):
    if isinstance(timestamp, str):
        try:
            # Mengonversi string timestamp ke datetime
            return datetime.strptime(timestamp, "%Y-%m-%d %H:%M:%S")  # Sesuaikan format dengan yang ada di DB
        except ValueError:
            return datetime.now()  # Jika gagal, kembalikan waktu sekarang
    return timestamp  # Jika sudah datetime, kembalikan tanpa perubahan


# Buat dictionary dengan format yang diinginkan
@app.route('/api/data', methods=['GET'])
def get_data():
    # Ambil data dari MongoDB
    data = collection.find()

    # Siapkan dictionary untuk mengirimkan response JSON
    formatted_data = {
        "data_suhumax": None,  # Tempat untuk suhu maksimum
        "data_suhumin": None,  # Tempat untuk suhu minimum
        "data_suhurata": None, # Tempat untuk suhu rata-rata
        "nilai_suhu_max_humid_max": []
    }

    # Inisialisasi variabel untuk perhitungan suhu tertinggi, terendah, dan total suhu
    suhu_max = float('-inf')  # Mulai dengan nilai yang sangat kecil
    suhu_min = float('inf')   # Mulai dengan nilai yang sangat besar
    suhu_total = 0
    count = 0

    # Memasukkan data yang sudah diformat ke dalam "nilai_suhu_max_humid_max"
    for item in data:
        # Dapatkan nilai suhu, dan update suhu_max dan suhu_min
        suhu = item.get("temperature", None)
        if suhu is not None:
            suhu_max = max(suhu_max, suhu)  # Tentukan suhu tertinggi
            suhu_min = min(suhu_min, suhu)  # Tentukan suhu terendah
            suhu_total += suhu
            count += 1

        formatted_item = {
            "idx": str(item.get("_id", None)),  # Convert ObjectId ke string
            "suhu": suhu,
            "humid": item.get("humidity", None),
            "kecerahan": item.get("ldr", None),
            "timestamp": convert_to_datetime(item.get("timestamp", datetime.now())).strftime("%Y-%m-%d %H.%M.%S")
        }
        formatted_data["nilai_suhu_max_humid_max"].append(formatted_item)

    # Menghitung suhu rata-rata jika ada data
    if count > 0:
        suhu_rata = suhu_total / count
    else:
        suhu_rata = 0

    # Memasukkan nilai suhu_max, suhu_min, dan suhu_rata ke dalam formatted_data
    formatted_data["data_suhumax"] = suhu_max
    formatted_data["data_suhumin"] = suhu_min
    formatted_data["data_suhurata"] = round(suhu_rata, 2)  # Membulatkan rata-rata suhu

    # Mengirimkan response JSON
    return jsonify(formatted_data), 200

# Jalankan Flask dengan SocketIO
if __name__ == "__main__":
    socketio.run(app, host="0.0.0.0", port=5000)
