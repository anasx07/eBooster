declare global {
  interface Window {
    chrome?: {
      webview?: {
        postMessage: (message: any) => void;
        addEventListener: (type: string, listener: (event: any) => void) => void;
        removeEventListener: (type: string, listener: (event: any) => void) => void;
      };
    };
  }
}

export function sendCommand(cmd: string, payload: Record<string, any> = {}) {
  const msg = JSON.stringify({ cmd, payload });
  if (window.chrome?.webview) {
    window.chrome.webview.postMessage(msg);
  } else {
    console.log(`[Dev Mock IPC] Sent -> ${cmd}`, payload);
  }
}

export function onNativeMessage(callback: (data: any) => void) {
  if (window.chrome?.webview) {
    const handler = (event: any) => {
      try {
        const data = typeof event.data === 'string' ? JSON.parse(event.data) : event.data;
        callback(data);
      } catch (err) {
        console.error('Failed to parse IPC message from C++', err);
      }
    };
    window.chrome.webview.addEventListener('message', handler);
    return () => window.chrome?.webview?.removeEventListener('message', handler);
  }
  return () => {};
}
