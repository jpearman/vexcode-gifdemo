MKDIR=-$(Q)${MKDIR_P} "$(@D)"

$(BUILD):
	$(MKDIR_P) $(BUILD)
	
$(BUILD)/%.o: %.c ${SRC_H}
	$(MKDIR)
	$(ECHO) "CC  $<"
	$(Q)$(CC) $(CFLAGS) $(INC) -c -o $@ $<
	
$(BUILD)/%.o: %.cpp ${SRC_H}
	$(MKDIR)
	$(ECHO) "CXX $<"
	$(Q)$(CXX) $(CXX_FLAGS) $(INC) -c -o $@ $<
	
$(BUILD)/$(PROJECT).bin: $(BUILD)/$(PROJECT).elf
	$(Q)$(OBJCOPY) -O binary $(BUILD)/$(PROJECT).elf $(BUILD)/$(PROJECT).bin

$(BUILD)/$(PROJECT).elf: $(OBJ)
	$(ECHO) "LINK $@"
	$(Q)$(LINK) $(LNK_FLAGS) -o $@ $^ $(LIBS)
	$(Q)$(SIZE) $@

clean:
ifeq ($(OS),Windows_NT)
	rmdir /S /Q $(BUILD)
else
	rm -rf $(BUILD)
endif
