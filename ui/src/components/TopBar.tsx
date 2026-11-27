import React from 'react';
import { Crosshair, Minus, X, ShieldCheck, ShieldAlert } from 'lucide-react';
import { sendCommand } from '../api';

interface TopBarProps {
  isBoosting: boolean;
  activeGame: string;
}

export const TopBar: React.FC<TopBarProps> = ({ isBoosting, activeGame }) => {
  const handleDrag = () => {
    sendCommand('window_drag');
  };

  return (
    <header 
      onMouseDown={handleDrag}
      className="h-12 bg-midnight-950/80 backdrop-blur-md border-b border-stadium-blue/20 flex items-center justify-between px-4 select-none cursor-move z-50"
    >
      {/* Brand & Logo */}
      <div className="flex items-center gap-3">
        <div className="relative flex items-center justify-center w-7 h-7 rounded-lg bg-stadium-blue/10 border border-stadium-blue/40 shadow-glow-blue">
          <Crosshair className="w-4 h-4 text-stadium-blue animate-pulse-slow" />
        </div>
        <div className="flex items-center gap-2">
          <span className="font-heading text-lg font-bold tracking-wider text-white">
            eBOOSTER
          </span>
          <span className="text-[10px] font-bold px-1.5 py-0.5 rounded bg-neon-magenta/20 text-neon-magenta border border-neon-magenta/50 neon-glow-magenta">
            PRO
          </span>
        </div>
      </div>

      {/* Center Status Badge */}
      <div className="flex items-center gap-2 text-xs font-mono px-3 py-1 rounded-full bg-midnight-850 border border-slate-700/60 shadow-inner">
        {isBoosting ? (
          <>
            <span className="w-2 h-2 rounded-full bg-neon-green animate-ping" />
            <ShieldCheck className="w-3.5 h-3.5 text-neon-green" />
            <span className="text-neon-green font-semibold">SHIELD ACTIVE: {activeGame}</span>
          </>
        ) : (
          <>
            <span className="w-2 h-2 rounded-full bg-slate-500" />
            <ShieldAlert className="w-3.5 h-3.5 text-slate-400" />
            <span className="text-slate-400">STANDBY MODE</span>
          </>
        )}
      </div>

      {/* Win32 Window Controls */}
      <div className="flex items-center gap-1" onMouseDown={e => e.stopPropagation()}>
        <button
          onClick={() => sendCommand('window_minimize')}
          className="w-8 h-8 rounded flex items-center justify-center text-slate-400 hover:text-white hover:bg-slate-800/80 transition-colors"
          title="Minimize to Tray"
        >
          <Minus className="w-4 h-4" />
        </button>
        <button
          onClick={() => sendCommand('window_close')}
          className="w-8 h-8 rounded flex items-center justify-center text-slate-400 hover:text-white hover:bg-neon-magenta/80 transition-colors"
          title="Close (Minimize to Tray)"
        >
          <X className="w-4 h-4" />
        </button>
      </div>
    </header>
  );
};
