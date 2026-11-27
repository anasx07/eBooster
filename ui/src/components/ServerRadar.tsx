import React from 'react';
import { Globe, Lock, CheckCircle2, RefreshCw } from 'lucide-react';
import { AppState, KonamiRegionNode } from '../types';
import { sendCommand } from '../api';

interface ServerRadarProps {
  state: AppState;
}

const FALLBACK_REGIONS: KonamiRegionNode[] = [
  { regionCode: 0, name: 'Auto (All Servers)', country: 'Global', desc: 'Default matchmaking across all Konami servers', ip: '0.0.0.0', ping: -1 },
  { regionCode: 1, name: 'Madrid [EU-Southwest]', country: 'Spain', desc: 'Konami Southern Europe Dedicated Cluster', ip: '185.60.114.159', ping: 35 },
  { regionCode: 2, name: 'Paris [EU-West]', country: 'France', desc: 'Konami Western Europe Dedicated Cluster', ip: '185.60.112.157', ping: 32 },
  { regionCode: 3, name: 'Amsterdam [EU-Central]', country: 'Netherlands', desc: 'Konami Benelux Dedicated Cluster', ip: '155.133.240.20', ping: 29 },
  { regionCode: 4, name: 'Frankfurt [EU-Central]', country: 'Germany', desc: 'Konami Central Europe Core Cluster', ip: '155.133.248.51', ping: 28 },
  { regionCode: 5, name: 'London [EU-Northwest]', country: 'United Kingdom', desc: 'Konami UK Dedicated Cluster', ip: '155.133.246.50', ping: 38 },
  { regionCode: 6, name: 'Milan [EU-South]', country: 'Italy', desc: 'Konami Italy Dedicated Cluster', ip: '185.60.115.10', ping: 41 },
  { regionCode: 8, name: 'Warsaw [EU-East]', country: 'Poland', desc: 'Konami Eastern Europe Cluster', ip: '155.133.242.15', ping: 46 },
  { regionCode: 17, name: 'Sao Paulo [South America]', country: 'Brazil', desc: 'Konami LATAM Cluster', ip: '205.185.194.3', ping: 145 },
  { regionCode: 22, name: 'Tokyo [Asia-East]', country: 'Japan', desc: 'Konami Japan Core Cluster', ip: '155.133.245.2', ping: 210 },
];

export const ServerRadar: React.FC<ServerRadarProps> = ({ state }) => {
  const serverNodes = (state.konamiRegions && state.konamiRegions.length > 0)
    ? state.konamiRegions
    : FALLBACK_REGIONS;

  const selectedRegion = state.prefs.selectedKonamiRegion;

  const handleSelectServer = (regionCode: number) => {
    sendCommand('select_server_region', { regionCode });
  };

  const handleToggleAutoLock = () => {
    sendCommand('toggle_server_autolock');
  };

  const handleBenchmark = () => {
    sendCommand('benchmark_servers');
  };

  return (
    <div className="flex-1 overflow-y-auto p-6 space-y-6">
      {/* Header Banner */}
      <div className="glass-panel rounded-2xl p-6 relative overflow-hidden flex flex-col sm:flex-row items-center justify-between gap-4">
        <div className="space-y-1">
          <div className="flex items-center gap-2">
            <Globe className="w-4 h-4 text-stadium-blue" />
            <span className="text-xs font-mono font-semibold text-stadium-blue uppercase tracking-wider">
              WFP Dedicated Server Geofilter
            </span>
          </div>
          <h2 className="text-2xl font-heading font-bold text-white">
            Konami & Esports Dedicated Server Lock
          </h2>
          <p className="text-xs text-slate-400 max-w-lg">
            Blocks matchmaking into high-latency, stutter-prone peer-to-peer or remote clusters.
            Forces the game to lock onto your chosen low-latency dedicated server node.
          </p>
        </div>

        <div className="flex items-center gap-3">
          <button
            onClick={handleBenchmark}
            className="flex items-center gap-2 px-3.5 py-2.5 rounded-xl bg-midnight-800 hover:bg-midnight-700 border border-slate-700 text-xs font-semibold text-slate-300 hover:text-white transition-colors"
            title="Ping all server clusters"
          >
            <RefreshCw className="w-3.5 h-3.5 text-stadium-blue" />
            <span>PING ALL</span>
          </button>

          <button
            onClick={handleToggleAutoLock}
            className={`flex items-center gap-2 px-4 py-2.5 rounded-xl border text-xs font-semibold transition-all ${
              state.prefs.efootballAutoLock
                ? 'bg-neon-magenta/20 border-neon-magenta text-neon-magenta shadow-glow-magenta'
                : 'bg-midnight-800 border-slate-700 text-slate-400 hover:text-white'
            }`}
          >
            <Lock className="w-3.5 h-3.5" />
            <span>{state.prefs.efootballAutoLock ? 'AUTO-LOCK ENGAGED' : 'AUTO-LOCK DISABLED'}</span>
          </button>
        </div>
      </div>

      {/* Server Clusters Grid */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
        {serverNodes.map(node => {
          const isSelected = selectedRegion === node.regionCode;
          const pingVal = node.ping > 0 ? Math.round(node.ping) : null;
          return (
            <div
              key={node.regionCode}
              onClick={() => handleSelectServer(node.regionCode)}
              className={`glass-panel rounded-xl p-4 cursor-pointer transition-all duration-200 relative overflow-hidden flex flex-col justify-between ${
                isSelected
                  ? 'border-stadium-blue bg-stadium-blue/10 shadow-glow-blue'
                  : 'hover:border-slate-600 hover:bg-slate-800/30'
              }`}
            >
              <div className="flex items-start justify-between">
                <div>
                  <h3 className="font-heading font-bold text-base text-white flex items-center gap-2">
                    <span>{node.name}</span>
                    {isSelected && <CheckCircle2 className="w-4 h-4 text-stadium-blue shrink-0" />}
                  </h3>
                  <span className="text-xs text-slate-400 font-mono">{node.country}</span>
                </div>

                {/* Ping Pill */}
                <div className={`px-2.5 py-1 rounded-md text-xs font-mono font-bold shrink-0 ${
                  pingVal === null
                    ? 'bg-slate-800 text-slate-400 border border-slate-700'
                    : pingVal < 40
                    ? 'bg-neon-green/20 text-neon-green border border-neon-green/30'
                    : pingVal < 75
                    ? 'bg-neon-amber/20 text-neon-amber border border-neon-amber/30'
                    : 'bg-red-500/20 text-red-400 border border-red-500/30'
                }`}>
                  {pingVal !== null ? `${pingVal} ms` : 'AUTO'}
                </div>
              </div>

              <div className="mt-4 pt-3 border-t border-slate-800 flex items-center justify-between text-[11px] text-slate-400 font-mono">
                <span>{node.ip && node.ip !== '0.0.0.0' ? `Cluster IP: ${node.ip}` : 'All Regions Allowed'}</span>
                <span className={isSelected ? 'text-stadium-blue font-bold' : 'text-slate-500'}>
                  {isSelected ? 'ACTIVE LOCK' : 'CLICK TO LOCK'}
                </span>
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
};
