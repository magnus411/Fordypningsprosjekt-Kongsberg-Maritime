from flask import Flask, jsonify, request
from flask_sqlalchemy import SQLAlchemy
from flask_socketio import SocketIO
from sqlalchemy import text
import os
from threading import Event, Thread
import random
import time

app = Flask(__name__)

app.config['SQLALCHEMY_DATABASE_URI'] = f'postgresql://postgress:123123123@172.23.246.166:5432/fordypningsprosjekt'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
db = SQLAlchemy(app)
socketio = SocketIO(app, cors_allowed_origins="*")

@app.route('/getTables', methods=['GET'])
def get_tables():
    """Fetch all table names in the database."""
    query = text("""SELECT table_name FROM information_schema.tables WHERE table_schema = 'public'""")
    result = db.session.execute(query)
    tables = [row[0] for row in result]
    return jsonify(tables=tables)

@app.route('/getTable/<table>', methods=['GET'])
def get_table(table):
    """Fetch all rows from a specified table."""
    try:
        query = text(f'SELECT * FROM {table}')
        result = db.session.execute(query)
        rows = [dict(row) for row in result]
        return jsonify(rows=rows)
    except Exception as e:
        return jsonify(error=str(e)), 400

@socketio.on('connect')
def handle_connect():
    print("Client connected")

@socketio.on('disconnect')
def handle_disconnect():
    print("Client disconnected")

def send_random_data():
    """Broadcast random data periodically."""
    stop_event = Event()

    def broadcast():
        while not stop_event.is_set():
            # Generate random data for RPM, Power, Torque
            data = {
                'RPM': random.randint(1000, 5000),
                'KW': random.randint(50, 200),
                'kNm': random.randint(100, 500)
            }
            socketio.emit('update', data)
            time.sleep(2)  # Broadcast data every 2 seconds

    thread = Thread(target=broadcast)
    thread.start()
    return stop_event

stop_event = send_random_data()

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5000)
