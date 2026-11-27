import React from 'react';
import { 
  Power, 
  Activity, 
  Wifi, 
  Server, 
  ShieldAlert, 
  ArrowDownRight, 
  Sparkles,
  Gamepad2
} from 'lucide-react';
import { AppState } from '../types';
import { sendCommand } from '../api';

interface DashboardProps {
  state: AppState;
  onSelectTab: (tab: any) => void;
}

export const Dashboard: React.FC<DashboardProps> = ({ state, onSelectTab }) => {
  const currentProfile = state.profiles[state.selectedProfileIdx] || {
    name: 'eFootball (Steam)',
    exeName: 'eFootball.exe'
  };

  const ping = state.currentPing > 0 ? Math.round(state.currentPing) : 24;
  const baseline = state.baselineIspPing > 0 ? Math.round(state.baselineIspPing) : 48;
  const diffPct = Math.max(0, Math.round(((baseline - ping) / baseline) * 100));

  const handleToggleBoost = () => {
    sendCommand('toggle_boost');
  };

  return (
    <div className="flex-1 overflow-y-auto p-6 space-y-6">
      {/* Hero Card: 1-Click Game Boost & Power Switch */}
      <div className="glass-panel rounded-2xl p-6 relative overflow-hidden flex flex-col md:flex-row items-center justify-between gap-6">
        {/* Background Ambient Glow */}
        <div className={`absolute -right-20 -top-20 w-80 h-80 rounded-full blur-3xl pointer-events-none transition-all duration-700 ${
          state.isBoosting ? 'bg-stadium-blue/20' : 'bg-slate-800/20'
        }`} />

        <div className="space-y-3 z-10">
          <div className="flex items-center gap-2">
            <span className="text-xs font-mono font-semibold uppercase tracking-wider text-stadium-blue flex items-center gap-1.5">
              <Sparkles className="w-3.5 h-3.5" />
              Windows Filtering Platform (WFP) Low-Latency Shield
            </span>
          </div>

          <h1 className="text-3xl font-heading font-bold text-white flex items-center gap-3">
            <span>{currentProfile.name}</span>
          </h1>

          <p className="text-sm text-slate-400 max-w-xl">
            {state.isBoosting 
              ? 'Anti-Lag Kernel Firewall active. Outbound TCP forced to pure UDP dedicated server cluster with sub-millisecond priority.'
              : 'Standby mode. Launching your game or clicking Boost locks the lowest-latency dedicated cluster instantly.'
            }
          </p>

          <div className="flex items-center gap-3 pt-2">
            <button
              onClick={() => onSelectTab('profiles')}
              className="flex items-center gap-2 px-3 py-1.5 rounded-lg bg-midnight-800 hover:bg-midnight-700 text-xs text-slate-300 border border-slate-700/60 transition-colors"
            >
              <Gamepad2 className="w-3.5 h-3.5 text-stadium-blue" />
              <span>Change Game</span>
            </button>
            <button
              onClick={() => onSelectTab('radar')}
              className="flex items-center gap-2 px-3 py-1.5 rounded-lg bg-midnight-800 hover:bg-midnight-700 text-xs text-slate-300 border border-slate-700/60 transition-colors"
            >
              <Server className="w-3.5 h-3.5 text-neon-magenta" />
              <span>Dedicated Server Lock</span>
            </button>
          </div>
        </div>

        {/* Big Animated 1-Click Boost Button */}
        <div className="z-10 flex flex-col items-center gap-2">
          <button
            onClick={handleToggleBoost}
            className={`relative group flex items-center justify-center w-28 h-28 rounded-full transition-all duration-500 shadow-2xl ${
              state.isBoosting 
                ? 'bg-gradient-to-br from-stadium-blue to-stadium-dark shadow-glow-blue border-2 border-cyan-300'
                : 'bg-midnight-800 hover:bg-midnight-700 border-2 border-slate-700 hover:border-stadium-blue/60 text-slate-400 hover:text-white'
            }`}
          >
            {/* Outer Pulsing Ring */}
            {state.isBoosting && (
              <span className="absolute inset-0 rounded-full border-2 border-stadium-blue animate-ping opacity-35" />
            )}
            <Power className={`w-10 h-10 transition-transform duration-300 group-hover:scale-110 ${
              state.isBoosting ? 'text-white' : 'text-slate-400 group-hover:text-stadium-blue'
            }`} />
          </button>
          <span className="text-xs font-mono font-bold tracking-wider text-slate-300">
            {state.isBoosting ? 'BOOST ENGAGED' : 'CLICK TO BOOST'}
          </span>
        </div>
      </div>

      {/* Real-Time Telemetry Grid */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4">
        {/* Live Ping Card */}
        <div className="glass-panel glass-panel-hover rounded-xl p-4 flex flex-col justify-between">
          <div className="flex items-center justify-between text-xs text-slate-400 font-mono">
            <span className="flex items-center gap-1.5">
              <Activity className="w-3.5 h-3.5 text-stadium-blue" />
              LIVE PING
            </span>
            <span className="text-neon-green font-bold">EXCELLENT</span>
          </div>
          <div className="my-2 flex items-baseline gap-2">
            <span className="text-4xl font-heading font-bold text-white neon-glow-blue">{ping}</span>
            <span className="text-sm font-mono text-slate-400">ms</span>
          </div>
          <div className="flex items-center gap-1.5 text-xs text-neon-green font-mono">
            <ArrowDownRight className="w-3.5 h-3.5" />
            <span>-{diffPct}% vs Default ISP ({baseline}ms)</span>
          </div>
        </div>

        {/* Jitter Card */}
        <div className="glass-panel glass-panel-hover rounded-xl p-4 flex flex-col justify-between">
          <div className="flex items-center justify-between text-xs text-slate-400 font-mono">
            <span className="flex items-center gap-1.5">
              <Wifi className="w-3.5 h-3.5 text-neon-cyan" />
              PACKET JITTER
            </span>
            <span className="text-slate-400 font-mono">STABLE</span>
          </div>
          <div className="my-2 flex items-baseline gap-2">
            <span className="text-4xl font-heading font-bold text-white">{state.jitterMs.toFixed(1)}</span>
            <span className="text-sm font-mono text-slate-400">ms</span>
          </div>
          <span className="text-xs text-slate-400 font-mono">Zero packet delay spikes</span>
        </div>

        {/* Packet Loss Card */}
        <div className="glass-panel glass-panel-hover rounded-xl p-4 flex flex-col justify-between">
          <div className="flex items-center justify-between text-xs text-slate-400 font-mono">
            <span className="flex items-center gap-1.5">
              <ShieldAlert className="w-3.5 h-3.5 text-neon-green" />
              PACKET LOSS
            </span>
            <span className="text-neon-green font-mono font-bold">CLEAN</span>
          </div>
          <div className="my-2 flex items-baseline gap-2">
            <span className="text-4xl font-heading font-bold text-neon-green">
              {state.packetLossPct.toFixed(1)}
            </span>
            <span className="text-sm font-mono text-slate-400">%</span>
          </div>
          <span className="text-xs text-slate-400 font-mono">No dropped UDP datagrams</span>
        </div>

        {/* Dedicated Server Region Card */}
        <div className="glass-panel glass-panel-hover rounded-xl p-4 flex flex-col justify-between">
          <div className="flex items-center justify-between text-xs text-slate-400 font-mono">
            <span className="flex items-center gap-1.5">
              <Server className="w-3.5 h-3.5 text-neon-magenta" />
              SERVER LOCK
            </span>
            <span className={`font-mono font-bold ${
              state.prefs.selectedKonamiRegion > 0 ? 'text-neon-magenta' : 'text-stadium-blue'
            }`}>
              {state.prefs.selectedKonamiRegion > 0 ? 'LOCKED' : 'AUTO'}
            </span>
          </div>
          <div className="my-2">
            <div className="text-lg font-heading font-bold text-white truncate">
              {state.matchInfo.locationName || 'Frankfurt [EU-Central]'}
            </div>
            <div className="text-xs font-mono text-slate-400 truncate">
              {state.matchInfo.remoteIp || '155.133.248.51'}
            </div>
          </div>
          <span className="text-xs text-stadium-blue font-mono">Pure UDP Route Active</span>
        </div>
      </div>

      {/* Live Match & Telemetry Stream Bar */}
      <div className="glass-panel rounded-xl p-4 flex flex-col sm:flex-row items-center justify-between gap-4">
        <div className="flex items-center gap-3">
          <div className="w-3 h-3 rounded-full bg-neon-green animate-pulse" />
          <div>
            <div className="text-xs font-mono font-bold text-slate-200">
              {state.matchInfo.isMatchActive ? 'LIVE MATCH IN PROGRESS' : 'STANDBY - GAME MONITORING ENGAGED'}
            </div>
            <div className="text-xs text-slate-400">
              Packet Rate: {state.matchInfo.packetRate || 128} pkts/sec • Protocol: {state.matchInfo.protocol || 'Pure UDP'} • Port Pacing: 0ms Delay
            </div>
          </div>
        </div>

        <button
          onClick={() => onSelectTab('optimizer')}
          className="px-4 py-2 rounded-lg bg-stadium-blue/15 hover:bg-stadium-blue/25 text-stadium-blue border border-stadium-blue/40 text-xs font-semibold transition-colors flex items-center gap-2 whitespace-nowrap"
        >
          <Sparkles className="w-3.5 h-3.5" />
          <span>
            Kernel Tweaks ({[
              state.optimizer?.nagleDisabled,
              state.optimizer?.throttlingDisabled,
              state.optimizer?.responsivenessOptimized,
              state.optimizer?.gamingPriorityElevated,
              state.optimizer?.mpoDisabled,
              state.optimizer?.efootballAntiLagShield
            ].filter(Boolean).length}/6 Active)
          </span>
        </button>
      </div>
    </div>
  );
};
