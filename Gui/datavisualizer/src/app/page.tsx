"use client";
import { useEffect, useState } from "react";
import io from "socket.io-client";
import { GaugeComponent } from "react-gauge-component";

const Dashboard = () => {
  const [rpm, setRpm] = useState(0);
  const [power, setPower] = useState(0);
  const [torque, setTorque] = useState(0);

  useEffect(() => {
    // Connect to the WebSocket server
    const socket = io("http://localhost:5000");

    // Listen for the "update" event
    socket.on("update", (data) => {
      setRpm(data.RPM);
      setPower(data.KW);
      setTorque(data.kNm);
      console.log(data);
    });

    // Cleanup on component unmount
    return () => {
      socket.disconnect();
    };
  }, []);

  return (
    <div className="min-h-screen bg-white">
      {/* Navbar */}
      <nav className="bg-white shadow-lg p-6 mb-10">
        <div className="max-w-7xl mx-auto flex items-center justify-between">
          <h1 className="text-3xl font-semibold text-black">Dashboard</h1>
          <ul className="flex space-x-8 text-indigo-600">
            <li>
              <a href="/" className="hover:text-indigo-800 transition-colors">
                Dashboard
              </a>
            </li>
            <li>
              <a
                href="/data"
                className="hover:text-indigo-800 transition-colors"
              >
                Data
              </a>
            </li>
          </ul>
        </div>
      </nav>

      {/* Dashboard Content */}
      <div className="max-w-7xl mx-auto px-6 lg:px-12">
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-12">
          {/* RPM Gauge */}
          <div className="flex flex-col items-center bg-white shadow-xl rounded-xl p-6">
            <h3 className="text-xl font-semibold text-black mb-4">RPM</h3>
            <GaugeComponent
              minValue={0}
              maxValue={3000}
              arc={{
                subArcs: [
                  { limit: 500, color: "#EA4228", showTick: true },
                  { limit: 1000, color: "#F58B19", showTick: true },
                  { limit: 2000, color: "#F5CD19", showTick: true },
                  { limit: 3000, color: "#5BE12C", showTick: true },
                ],
              }}
              value={rpm}
              needleColor="#333"
              animationSpeed={50}
            />
          </div>

          {/* Power Gauge */}
          <div className="flex flex-col items-center bg-white shadow-xl rounded-xl p-6">
            <h3 className="text-xl font-semibold text-black mb-4">
              Power (KW)
            </h3>
            <GaugeComponent
              minValue={0}
              maxValue={10000}
              arc={{
                subArcs: [
                  { limit: 2000, color: "#EA4228", showTick: true },
                  { limit: 4000, color: "#F58B19", showTick: true },
                  { limit: 6000, color: "#F5CD19", showTick: true },
                  { limit: 10000, color: "#5BE12C", showTick: true },
                ],
              }}
              value={power}
              needleColor="#333"
              animationSpeed={50}
            />
          </div>

          {/* Torque Gauge */}
          <div className="flex flex-col items-center bg-white shadow-xl rounded-xl p-6">
            <h3 className="text-xl font-semibold text-black mb-4">
              Torque (kNm)
            </h3>
            <GaugeComponent
              minValue={0}
              maxValue={1000}
              arc={{
                subArcs: [
                  { limit: 200, color: "#EA4228", showTick: true },
                  { limit: 400, color: "#F58B19", showTick: true },
                  { limit: 600, color: "#F5CD19", showTick: true },
                  { limit: 1000, color: "#5BE12C", showTick: true },
                ],
              }}
              value={torque}
              needleColor="#333"
              animationSpeed={50}
            />
          </div>
        </div>
      </div>
    </div>
  );
};

export default Dashboard;
