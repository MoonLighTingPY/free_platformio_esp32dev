# Gateway Timer Script - runs every few seconds to control fans based on mode

def calculate_fan_logic(temp, setpoint):
    """
    Calculate fan state and speed based on temperature delta
    Returns: (state, speed, cooling)
    """
    delta = temp - setpoint
    
    if delta <= 0:
        return (False, 0, 0.0) 
    elif delta < 5:
        return (True, 1, 0.3)   
    elif delta < 10:
        return (True, 2, 0.6)   
    else:
        return (True, 3, 0.9)   

# Read current temperature and control settings from ESP32 MQTT data
tags_to_read = [
    '[MQTT Engine]ventilation/ventilation/temp',       
    '[MQTT Engine]ventilation/ventilation/mode',          
    '[MQTT Engine]ventilation/ventilation/sp1',     
    '[MQTT Engine]ventilation/ventilation/sp2',     
    '[MQTT Engine]ventilation/ventilation/sp3',
    '[MQTT Engine]ventilation/ventilation/eco_sp'
]

try:
    tag_values = system.tag.readBlocking(tags_to_read)
    temp = tag_values[0].value
    mode = tag_values[1].value
    sp1 = tag_values[2].value
    sp2 = tag_values[3].value
    sp3 = tag_values[4].value
    eco_sp = tag_values[5].value
except Exception as e:
    system.util.getLogger("VentilationControl").error("Error reading tags: " + str(e))
    # Exit early if we can't read tags
    import sys
    sys.exit()

# Tags to write fan states and speeds (published to MQTT)
tags_to_write_state = [
    '[default]MQTT Tags/fan1_state',
    '[default]MQTT Tags/fan2_state',
    '[default]MQTT Tags/fan3_state'
]

tags_to_write_speed = [
    '[default]MQTT Tags/fan1_speed',
    '[default]MQTT Tags/fan2_speed',
    '[default]MQTT Tags/fan3_speed'
]

# Initialize values
values_to_write_state = [False, False, False] 
values_to_write_speed = [0, 0, 0]

# Mode 0: Manual control - do nothing, let user control states directly
if mode == 0:
    # Don't override anything, exit early
    pass

# Mode 1: Automatic - each fan controlled by its own setpoint
elif mode == 1:
    # Turn on fans only if temperature is ABOVE their setpoint
    fan1_logic = calculate_fan_logic(temp, sp1)
    fan2_logic = calculate_fan_logic(temp, sp2)
    fan3_logic = calculate_fan_logic(temp, sp3)
    
    # Invert the state in mode 1
    values_to_write_state = [not fan1_logic[0], not fan2_logic[0], not fan3_logic[0]]
    values_to_write_speed = [fan1_logic[1], fan2_logic[1], fan3_logic[1]]
    
    try:
        system.tag.writeBlocking(tags_to_write_state, values_to_write_state)
        system.tag.writeBlocking(tags_to_write_speed, values_to_write_speed)
    except Exception as e:
        system.util.getLogger("VentilationControl").error("Error writing fan states (Mode 1): " + str(e))

# Mode 2 or 3: Economy mode - all fans use eco setpoint
elif mode == 2 or mode == 3:
    fan_logic = calculate_fan_logic(temp, eco_sp) 
    
    # Invert the state for all fans in economy mode (Mode 2 only)
    if mode == 2:
        values_to_write_state = [not fan_logic[0], not fan_logic[0], not fan_logic[0]]
    else:
        values_to_write_state = [fan_logic[0], fan_logic[0], fan_logic[0]]
    values_to_write_speed = [fan_logic[1], fan_logic[1], fan_logic[1]]
    
    try:
        system.tag.writeBlocking(tags_to_write_state, values_to_write_state)
        system.tag.writeBlocking(tags_to_write_speed, values_to_write_speed)
    except Exception as e:
        system.util.getLogger("VentilationControl").error("Error writing fan states (Economy): " + str(e))