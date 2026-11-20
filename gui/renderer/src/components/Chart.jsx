import { useEffect, useRef } from 'react';
import * as echarts from 'echarts/core';
import { PieChart, BarChart, GraphChart } from 'echarts/charts';
import { TooltipComponent, LegendComponent, GridComponent } from 'echarts/components';
import { CanvasRenderer } from 'echarts/renderers';

echarts.use([PieChart, BarChart, GraphChart, TooltipComponent, LegendComponent, GridComponent, CanvasRenderer]);

export default function Chart({ option, height = 220, className = '', onEvents }) {
  const hostRef = useRef(null);
  const chartRef = useRef(null);

  useEffect(() => {
    if (!hostRef.current) return undefined;
    const chart = echarts.init(hostRef.current, null, { renderer: 'canvas' });
    chartRef.current = chart;
    const ro = new ResizeObserver(() => {
      try { chart.resize(); } catch { /* disposed */ }
    });
    ro.observe(hostRef.current);
    return () => {
      ro.disconnect();
      chart.dispose();
      chartRef.current = null;
    };
  }, []);

  useEffect(() => {
    if (!chartRef.current || !option) return;
    chartRef.current.setOption(option, true);
  }, [option]);

  useEffect(() => {
    const chart = chartRef.current;
    if (!chart || !onEvents) return undefined;
    for (const [event, handler] of Object.entries(onEvents)) chart.on(event, handler);
    return () => {
      for (const [event, handler] of Object.entries(onEvents)) chart.off(event, handler);
    };
  }, [onEvents]);

  return <div ref={hostRef} className={`chart-host ${className}`} style={{ height }} />;
}
