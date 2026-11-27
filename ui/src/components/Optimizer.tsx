import React from 'react';
import { 
  Zap, 
  CheckCircle2, 
  RotateCcw, 
  Sliders, 
  Cpu, 
  Network, 
  Activity, 
  Layers, 
  ShieldCheck
} from 'lucide-react';
import { AppState } from '../types';
import { sendCommand } from '../api';

interface OptimizerProps {
  state: AppState;
}

export const Optimizer: React.FC<OptimizerProps> = ({ state }) => {
  const opt = state.optimizer || {
    nagleDisabled: false,
    throttlingDisabled: false,
    responsivenessOptimized: false,
    gamingPriorityElevated: false,
    mpoDisabled: false,
    efootballAntiLagShield: false,
    optimizedAdaptersCount: 0
  };

  const handleApplyAll = () => {
    sendCommand('apply_all_tweaks');
  };

  const handleRevertAll = () => {
    sendCommand('revert_all_tweaks');
  };

  const handleToggleTweak = (tweakKey: string) => {
    sendCommand('toggle_tweak', { tweak: tweakKey });
  };

  const tweaks = [
    {
      key: 'nagle',
      name: "Nagle's Packet Packing Elimination",
      desc: 'Sets TcpAckFrequency=1 & TCPNoDelay=1. Instantly transmits small game input packets without buffering delay.',
      active: opt.nagleDisabled,
      icon: Network,
      tag: 'Kernel TCP/IP'
    },
    {
      key: 'throttling',
      name: 'Windows Network Throttling Elimination',
      desc: 'Sets NetworkThrottlingIndex=0xFFFFFFFF. Disables Windows 10/11 10-packet-per-millisecond throughput cap on non-multimedia traffic.',
      active: opt.throttlingDisabled,
      icon: Sliders,
      tag: 'System Multimedia'
    },
    {
      key: 'responsiveness',
      name: 'System Responsiveness Gaming Priority',
      desc: 'Sets SystemResponsiveness=0. Reclaims the 20% background Windows reserve and dedicates 100% CPU time slices to active game loops.',
      active: opt.responsivenessOptimized,
      icon: Cpu,
      tag: 'Kernel Scheduler'
    },
    {
      key: 'priority',
      name: 'Multimedia Game Scheduling Elevation',
      desc: 'Elevates games to High Scheduling Category and Priority=6 in MMCSS (Multimedia Class Scheduler Service).',
      active: opt.gamingPriorityElevated,
      icon: Activity,
      tag: 'MMCSS Gaming'
    },
    {
      key: 'mpo',
      name: 'DWM Multiplane Overlay (MPO) Stutter Fix',
      desc: 'Sets OverlayTestMode=5 to prevent GPU driver multiplane desync and black screen micro-stutters during borderless gaming.',
      active: opt.mpoDisabled,
      icon: Layers,
      tag: 'DirectX / DWM'
    },
    {
      key: 'anticheat',
      name: 'eFootball Anti-TCP Lag Shield',
      desc: 'Windows Filtering Platform rule that blocks peer-to-peer TCP fallback, guaranteeing pure low-latency UDP dedicated servers.',
      active: opt.efootballAntiLagShield,
      icon: ShieldCheck,
      tag: 'WFP Firewall'
    }
  ];

  const totalActive = tweaks.filter(t => t.active).length;

  return (
    <div className="flex-1 overflow-y-auto p-6 space-y-6">
      {/* Header Banner */}
      <div className="glass-panel rounded-2xl p-6 relative overflow-hidden flex flex-col md:flex-row items-center justify-between gap-6">
        <div className="space-y-2">
          <div className="flex items-center gap-2">
            <Zap className="w-4 h-4 text-stadium-blue" />
            <span className="text-xs font-mono font-semibold text-stadium-blue uppercase tracking-wider">
              Windows Gaming Kernel Latency Optimization
            </span>
          </div>
          <h2 className="text-2xl font-heading font-bold text-white">
            1-Click Windows Network & System Tuning
          </h2>
          <p className="text-xs text-slate-400 max-w-xl">
            Fine-tunes the Windows TCP/IP stack, eliminates background network packet throttling, 
            and optimizes GPU/CPU scheduling priorities specifically for esports titles.
          </p>

          <div className="flex items-center gap-4 pt-2 text-xs font-mono">
            <span className="text-slate-400">
              Active Tweaks: <strong className="text-white">{totalActive} / {tweaks.length}</strong>
            </span>
            <span className="text-slate-400">
              Active Adapters: <strong className="text-stadium-blue">{opt.optimizedAdaptersCount || 1}</strong>
            </span>
          </div>
        </div>

        {/* Global Action Buttons */}
        <div className="flex flex-col sm:flex-row items-center gap-3">
          <button
            onClick={handleApplyAll}
            className="w-full sm:w-auto px-5 py-3 rounded-xl bg-gradient-to-r from-stadium-blue to-cyan-500 hover:from-cyan-400 hover:to-stadium-blue text-white font-semibold text-xs tracking-wide shadow-glow-blue transition-all flex items-center justify-center gap-2"
          >
            <CheckCircle2 className="w-4 h-4" />
            <span>APPLY ALL TWEAKS</span>
          </button>
          <button
            onClick={handleRevertAll}
            className="w-full sm:w-auto px-4 py-3 rounded-xl bg-midnight-800 hover:bg-midnight-700 text-slate-300 border border-slate-700 text-xs font-medium transition-colors flex items-center justify-center gap-2"
          >
            <RotateCcw className="w-3.5 h-3.5" />
            <span>REVERT TO DEFAULTS</span>
          </button>
        </div>
      </div>

      {/* Safety Notice Card */}
      <div className="flex items-center gap-3 p-3.5 rounded-xl bg-stadium-blue/5 border border-stadium-blue/20 text-xs text-slate-300">
        <ShieldCheck className="w-4 h-4 text-neon-green shrink-0" />
        <p>
          <strong>100% Non-Destructive:</strong> All settings modify standard Windows registry values under <code className="text-stadium-blue">Tcpip\Parameters</code> and <code className="text-stadium-blue">SystemProfile</code>. Zero system files are altered.
        </p>
      </div>

      {/* Tweaks Grid */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        {tweaks.map(t => {
          const Icon = t.icon;
          return (
            <div
              key={t.key}
              onClick={() => handleToggleTweak(t.key)}
              className={`glass-panel rounded-xl p-5 cursor-pointer transition-all duration-200 border flex flex-col justify-between ${
                t.active 
                  ? 'border-stadium-blue/60 bg-stadium-blue/10 shadow-glow-blue' 
                  : 'border-slate-800 hover:border-slate-700 hover:bg-slate-800/30'
              }`}
            >
              <div className="space-y-3">
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-2.5">
                    <div className={`p-2 rounded-lg ${t.active ? 'bg-stadium-blue/20 text-stadium-blue' : 'bg-slate-800 text-slate-400'}`}>
                      <Icon className="w-4 h-4" />
                    </div>
                    <span className="text-[10px] font-mono px-2 py-0.5 rounded bg-midnight-900 border border-slate-700 text-slate-400">
                      {t.tag}
                    </span>
                  </div>

                  {/* Switch Pill */}
                  <span className={`text-xs font-mono font-bold px-2 py-1 rounded ${
                    t.active 
                      ? 'bg-neon-green/20 text-neon-green border border-neon-green/50' 
                      : 'bg-slate-800 text-slate-500'
                  }`}>
                    {t.active ? 'ENABLED' : 'DEFAULT'}
                  </span>
                </div>

                <div>
                  <h3 className="font-heading font-bold text-sm text-white">{t.name}</h3>
                  <p className="text-xs text-slate-400 mt-1 leading-relaxed">{t.desc}</p>
                </div>
              </div>

              <div className="pt-4 mt-2 border-t border-slate-800/60 flex items-center justify-between text-[11px] text-slate-500">
                <span>Status: {t.active ? 'Optimized for Gaming' : 'Windows Stock Setting'}</span>
                <span className="text-stadium-blue hover:underline">Click to toggle</span>
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
};
