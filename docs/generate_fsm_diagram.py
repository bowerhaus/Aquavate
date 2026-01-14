#!/usr/bin/env python3
"""
Aquavate FSM Diagram Generator
Generates a comprehensive hierarchical finite state machine diagram
showing all system states, subsystem states, and transitions.

Requirements:
    pip install graphviz

Usage:
    python3 generate_fsm_diagram.py
"""

from graphviz import Digraph

def create_fsm_diagram():
    """Create comprehensive hierarchical FSM diagram for Aquavate firmware"""

    # Create main graph with optimized settings for hierarchical layout
    dot = Digraph(comment='Aquavate Firmware State Machine',
                  format='png',
                  engine='dot')

    # Graph attributes for professional appearance
    dot.attr(rankdir='TB',  # Top to bottom layout
             splines='spline',  # Curved edges (better for complex diagrams)
             nodesep='0.8',
             ranksep='1.0',
             fontname='Arial',
             fontsize='12',
             bgcolor='white')

    # Default node styling
    dot.attr('node',
             shape='rectangle',
             style='rounded,filled',
             fontname='Arial',
             fontsize='11',
             margin='0.3,0.2')

    # Default edge styling
    dot.attr('edge',
             fontname='Arial',
             fontsize='10',
             color='#333333')

    # ===========================================================================
    # MASTER SYSTEM STATE MACHINE (Top Level)
    # ===========================================================================

    with dot.subgraph(name='cluster_system') as system:
        system.attr(label='System State Machine (Master FSM)',
                   style='rounded,filled',
                   fillcolor='#E8F4F8',
                   color='#2C5F7C',
                   penwidth='2',
                   fontsize='14',
                   fontname='Arial Bold')

        # System states
        system.node('STARTUP', 'SYSTEM_STARTUP\n(Boot & Init)',
                   fillcolor='#B8E6F0', color='#2C5F7C', penwidth='2')
        system.node('NORMAL', 'SYSTEM_NORMAL_OPERATION\n(Drink Tracking)',
                   fillcolor='#90EE90', color='#228B22', penwidth='2')
        system.node('CALIBRATION', 'SYSTEM_CALIBRATION\n(Two-Point Cal)',
                   fillcolor='#FFD700', color='#B8860B', penwidth='2')
        system.node('DEEP_SLEEP', 'SYSTEM_DEEP_SLEEP\n(Future)',
                   fillcolor='#D3D3D3', color='#696969', penwidth='2', style='rounded,filled,dashed')

    # System state transitions
    dot.edge('STARTUP', 'NORMAL',
            label='setup() complete', color='#2C5F7C', penwidth='2')
    dot.edge('NORMAL', 'CALIBRATION',
            label='INVERTED_HOLD\n(5s)', color='#B8860B', penwidth='2')
    dot.edge('CALIBRATION', 'NORMAL',
            label='CAL_COMPLETE\nor CAL_ERROR', color='#228B22', penwidth='2')
    dot.edge('NORMAL', 'DEEP_SLEEP',
            label='inactivity timeout\n(future)', color='#696969', style='dashed')
    dot.edge('DEEP_SLEEP', 'NORMAL',
            label='wake interrupt\n(future)', color='#696969', style='dashed')

    # ===========================================================================
    # CALIBRATION SUB-FSM
    # ===========================================================================

    with dot.subgraph(name='cluster_calibration') as cal:
        cal.attr(label='Calibration State Machine',
                style='rounded,filled',
                fillcolor='#FFF8DC',
                color='#B8860B',
                penwidth='2',
                fontsize='13',
                fontname='Arial Bold')

        # Calibration states
        cal.node('CAL_IDLE', 'CAL_IDLE',
                fillcolor='#FFFACD', color='#B8860B')
        cal.node('CAL_TRIGGERED', 'CAL_TRIGGERED',
                fillcolor='#FFE4B5', color='#B8860B')
        cal.node('CAL_WAIT_EMPTY', 'CAL_WAIT_EMPTY\n(Place empty bottle)',
                fillcolor='#FFE4B5', color='#B8860B')
        cal.node('CAL_MEASURE_EMPTY', 'CAL_MEASURE_EMPTY\n(10s)',
                fillcolor='#FFE4B5', color='#B8860B')
        cal.node('CAL_CONFIRM_EMPTY', 'CAL_CONFIRM_EMPTY\n(Tilt sideways)',
                fillcolor='#FFE4B5', color='#B8860B')
        cal.node('CAL_WAIT_FULL', 'CAL_WAIT_FULL\n(Fill to 830ml)',
                fillcolor='#FFE4B5', color='#B8860B')
        cal.node('CAL_MEASURE_FULL', 'CAL_MEASURE_FULL\n(10s)',
                fillcolor='#FFE4B5', color='#B8860B')
        cal.node('CAL_CONFIRM_FULL', 'CAL_CONFIRM_FULL\n(Tilt sideways)',
                fillcolor='#FFE4B5', color='#B8860B')
        cal.node('CAL_COMPLETE', 'CAL_COMPLETE\n✓',
                fillcolor='#90EE90', color='#228B22', penwidth='2')
        cal.node('CAL_ERROR', 'CAL_ERROR\n✗',
                fillcolor='#FFB6C1', color='#DC143C', penwidth='2')

    # Calibration transitions
    dot.edge('CAL_IDLE', 'CAL_TRIGGERED', label='calibrationStart()', fontsize='9')
    dot.edge('CAL_TRIGGERED', 'CAL_WAIT_EMPTY', label='entry', fontsize='9')
    dot.edge('CAL_WAIT_EMPTY', 'CAL_MEASURE_EMPTY', label='UPRIGHT_STABLE', fontsize='9')
    dot.edge('CAL_MEASURE_EMPTY', 'CAL_CONFIRM_EMPTY', label='10s elapsed', fontsize='9')
    dot.edge('CAL_CONFIRM_EMPTY', 'CAL_WAIT_FULL', label='SIDEWAYS_TILT', fontsize='9')
    dot.edge('CAL_WAIT_FULL', 'CAL_MEASURE_FULL', label='UPRIGHT_STABLE', fontsize='9')
    dot.edge('CAL_MEASURE_FULL', 'CAL_CONFIRM_FULL', label='10s elapsed', fontsize='9')
    dot.edge('CAL_CONFIRM_FULL', 'CAL_COMPLETE', label='SIDEWAYS_TILT', fontsize='9')
    dot.edge('CAL_COMPLETE', 'CAL_IDLE', label='save & return', fontsize='9')

    # Error transitions
    dot.edge('CAL_WAIT_EMPTY', 'CAL_ERROR', label='timeout', fontsize='9', color='#DC143C', style='dashed')
    dot.edge('CAL_MEASURE_EMPTY', 'CAL_ERROR', label='unstable', fontsize='9', color='#DC143C', style='dashed')
    dot.edge('CAL_WAIT_FULL', 'CAL_ERROR', label='timeout', fontsize='9', color='#DC143C', style='dashed')
    dot.edge('CAL_MEASURE_FULL', 'CAL_ERROR', label='unstable', fontsize='9', color='#DC143C', style='dashed')
    dot.edge('CAL_ERROR', 'CAL_IDLE', label='5s delay', fontsize='9')

    # ===========================================================================
    # DRINK TRACKING SUB-FSM
    # ===========================================================================

    with dot.subgraph(name='cluster_drinks') as drinks:
        drinks.attr(label='Drink Tracking State Machine (drinksUpdate)',
                   style='rounded,filled',
                   fillcolor='#E8F5E9',
                   color='#228B22',
                   penwidth='2',
                   fontsize='13',
                   fontname='Arial Bold')

        # Drink tracking states with more detail
        drinks.node('DRINK_UNINIT', 'DRINK_UNINITIALIZED\n(time not set)\n\nWaiting for SET_TIME',
                   fillcolor='#FFB6C1', color='#DC143C')
        drinks.node('DRINK_BASELINE', 'DRINK_ESTABLISHING_BASELINE\n(waiting for first stable reading)\n\nlast_recorded_adc = 0',
                   fillcolor='#C8E6C9', color='#228B22')
        drinks.node('DRINK_MONITOR', 'DRINK_MONITORING\n(normal state)\n\nCalculate delta_ml\nbaseline_ml - current_ml',
                   fillcolor='#90EE90', color='#228B22', penwidth='2')
        drinks.node('DRINK_AGGREGATE', 'DRINK_AGGREGATING\n(5-minute aggregation window)\n\nWindow active, accumulating sips\nDisplay updates every ≥50ml\nSave records to NVS circular buffer',
                   fillcolor='#66BB6A', color='#1B5E20')

        # Decision node inside MONITORING state
        drinks.node('DECISION_DELTA', 'Evaluate\ndelta_ml',
                   shape='diamond', fillcolor='#C8E6C9', color='#228B22', width='1.2', height='1.2')

    # Drink tracking transitions with more detail
    dot.edge('DRINK_UNINIT', 'DRINK_BASELINE',
            label='drinksOnTimeSet()\n[time becomes valid]', fontsize='9', color='#228B22', penwidth='1.5')
    dot.edge('DRINK_BASELINE', 'DRINK_MONITOR',
            label='UPRIGHT_STABLE\n[first baseline captured]\nlast_recorded_adc = current_adc',
            fontsize='9', color='#228B22', penwidth='1.5')

    # MONITORING → Decision node (on UPRIGHT_STABLE event)
    dot.edge('DRINK_MONITOR', 'DECISION_DELTA',
            label='UPRIGHT_STABLE',
            fontsize='9', color='#228B22', penwidth='1.5')

    # Decision node outcomes
    # Branch 1: Drink detected
    dot.edge('DECISION_DELTA', 'DRINK_AGGREGATE',
            label='[delta ≥ 30ml]\nDrink detected\nCreate drink record\ndaily_total_ml += amount\nStart 5-min window',
            fontsize='9', color='#228B22', penwidth='1.5')

    # Branch 2: Refill detected
    dot.edge('DECISION_DELTA', 'DRINK_MONITOR',
            label='[delta ≤ -100ml]\nRefill detected\nUpdate baseline\nNo state change',
            fontsize='9', color='#2E7D32', penwidth='1.5')

    # Branch 3: Drift compensation
    dot.edge('DECISION_DELTA', 'DRINK_MONITOR',
            label='[|delta| < 30ml]\nDrift compensation\nUpdate baseline\nNo state change',
            fontsize='8', color='#66BB6A', style='dotted', penwidth='1.5')

    # Window closes
    dot.edge('DRINK_AGGREGATE', 'DRINK_MONITOR',
            label='5 minutes elapsed\n[window closes]\nSave final drink record',
            fontsize='9', color='#228B22', penwidth='1.5')

    # Additional sips during aggregation (same decision logic in AGGREGATING state)
    dot.edge('DRINK_AGGREGATE', 'DRINK_AGGREGATE',
            label='UPRIGHT_STABLE\n[delta ≥ 30ml within window]\nAggregate into existing record\ndaily_total_ml += amount',
            fontsize='9', color='#1B5E20', penwidth='1.5')

    # Refill during aggregation
    dot.edge('DRINK_AGGREGATE', 'DRINK_AGGREGATE',
            label='UPRIGHT_STABLE\n[delta ≤ -100ml]\nRefill detected\nUpdate baseline',
            fontsize='9', color='#2E7D32', constraint='false')

    # Daily reset (from any state)
    dot.edge('DRINK_MONITOR', 'DRINK_BASELINE',
            label='First drink after 4am\n[handleDailyReset()]\nReset counters atomically\nClear aggregation window\nlast_recorded_adc = 0',
            fontsize='9', color='#DC143C', style='dashed', penwidth='1.5', constraint='false')
    dot.edge('DRINK_AGGREGATE', 'DRINK_BASELINE',
            label='First drink after 4am\n[handleDailyReset()]\nReset counters atomically\nClear aggregation window\nlast_recorded_adc = 0',
            fontsize='9', color='#DC143C', style='dashed', penwidth='1.5', constraint='false')

    # ===========================================================================
    # GESTURE DETECTION (Event Source)
    # ===========================================================================

    with dot.subgraph(name='cluster_gestures') as gestures:
        gestures.attr(label='Gesture Detection (Event Source)',
                     style='rounded,filled',
                     fillcolor='#F3E5F5',
                     color='#6A1B9A',
                     penwidth='2',
                     fontsize='13',
                     fontname='Arial Bold')

        # Gesture types (not states - these are events)
        gestures.node('GEST_NONE', 'GESTURE_NONE\n(default)',
                     shape='oval', fillcolor='#E1BEE7', color='#6A1B9A')
        gestures.node('GEST_INVERTED', 'GESTURE_INVERTED_HOLD\n(Z < -0.8g, 5s)\n+ 2s cooldown',
                     shape='oval', fillcolor='#CE93D8', color='#6A1B9A', penwidth='2')
        gestures.node('GEST_UPRIGHT', 'GESTURE_UPRIGHT\n(Z > 0.97g)',
                     shape='oval', fillcolor='#CE93D8', color='#6A1B9A')
        gestures.node('GEST_STABLE', 'GESTURE_UPRIGHT_STABLE\n(upright + weight stable 2s)',
                     shape='oval', fillcolor='#BA68C8', color='#6A1B9A', penwidth='2')
        gestures.node('GEST_SIDEWAYS', 'GESTURE_SIDEWAYS_TILT\n(|X| or |Y| > 0.5g)',
                     shape='oval', fillcolor='#CE93D8', color='#6A1B9A')

    # Gesture flow (accelerometer → detection)
    gestures.node('ACCEL_INPUT', 'LIS3DH\nAccelerometer',
                 shape='box', fillcolor='#F8BBD0', color='#C2185B')
    dot.edge('ACCEL_INPUT', 'GEST_NONE', label='gesturesUpdate()', fontsize='9', color='#6A1B9A')

    # ===========================================================================
    # LEGEND
    # ===========================================================================

    with dot.subgraph(name='cluster_legend') as legend:
        legend.attr(label='Legend',
                   style='rounded',
                   color='#999999',
                   fontsize='12',
                   fontname='Arial Bold')

        legend.node('leg_system', 'System State',
                   fillcolor='#B8E6F0', color='#2C5F7C', shape='rectangle', style='rounded,filled')
        legend.node('leg_cal', 'Calibration State',
                   fillcolor='#FFE4B5', color='#B8860B', shape='rectangle', style='rounded,filled')
        legend.node('leg_drink', 'Drink Tracking State',
                   fillcolor='#90EE90', color='#228B22', shape='rectangle', style='rounded,filled')
        legend.node('leg_gesture', 'Gesture Event',
                   fillcolor='#CE93D8', color='#6A1B9A', shape='oval', style='filled')
        legend.node('leg_future', 'Future Feature',
                   fillcolor='#D3D3D3', color='#696969', shape='rectangle', style='rounded,filled,dashed')

        # Invisible edges to force vertical layout
        legend.edge('leg_system', 'leg_cal', style='invis')
        legend.edge('leg_cal', 'leg_drink', style='invis')
        legend.edge('leg_drink', 'leg_gesture', style='invis')
        legend.edge('leg_gesture', 'leg_future', style='invis')

    return dot


def main():
    """Generate and save the FSM diagram"""
    print("Generating Aquavate FSM diagram...")

    dot = create_fsm_diagram()

    # Save source code
    output_file = 'aquavate_fsm_diagram'
    dot.save(output_file + '.gv')
    print(f"✓ Saved Graphviz source: {output_file}.gv")

    # Render PNG
    dot.render(output_file, cleanup=True)
    print(f"✓ Saved diagram: {output_file}.png")

    print("\nDiagram generated successfully!")
    print("\nTo regenerate after code changes:")
    print("  python3 generate_fsm_diagram.py")


if __name__ == '__main__':
    main()
