CC                    := gcc
BIN                   := posix_demo

BUILD_DIR             := ./build
BUILD_DIR_ABS         := $(abspath $(BUILD_DIR))

FREERTOS_DIR_REL      := ../../../FreeRTOS
FREERTOS_DIR          := $(abspath $(FREERTOS_DIR_REL))

FREERTOS_PLUS_DIR_REL := ../../../FreeRTOS-Plus
FREERTOS_PLUS_DIR     := $(abspath $(FREERTOS_PLUS_DIR_REL))

KERNEL_DIR            := ${FREERTOS_DIR}/Source

INCLUDE_DIRS          := -I.
INCLUDE_DIRS          += -I${KERNEL_DIR}/include
INCLUDE_DIRS          += -I${KERNEL_DIR}/portable/ThirdParty/GCC/Posix
INCLUDE_DIRS          += -I${KERNEL_DIR}/portable/ThirdParty/GCC/Posix/utils
INCLUDE_DIRS          += -I${FREERTOS_DIR}/Demo/Common/include

# --------------------------------------------------------
# LOGICA DI SELEZIONE DEMO
# --------------------------------------------------------

# Default: se non scrivi nulla, usa ENCODER_DEMO
USER_DEMO ?= ENCODER_DEMO

ifeq ($(USER_DEMO),ENCODER_DEMO)
    # Se scrivi make USER_DEMO=ENCODER_DEMO
    USER_SRC := main_encoder.c
else ifeq ($(USER_DEMO),BLINKY_DEMO)
    # Esempio: se un domani vuoi usare blinky
    USER_SRC := main_blinky.c
else
    # Se scrivi un nome sbagliato, errore
    $(error Demo non riconosciuta! Usa make USER_DEMO=ENCODER_DEMO)
endif

# --------------------------------------------------------
# SOURCE FILES
# --------------------------------------------------------

# 1. Il file selezionato sopra
SOURCE_FILES          := $(USER_SRC)

# 2. Kernel e Porting
SOURCE_FILES          += $(wildcard ${FREERTOS_DIR}/Source/*.c)
SOURCE_FILES          += ${KERNEL_DIR}/portable/MemMang/heap_3.c
SOURCE_FILES          += ${KERNEL_DIR}/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c
SOURCE_FILES          += ${KERNEL_DIR}/portable/ThirdParty/GCC/Posix/port.c

# --------------------------------------------------------
# FLAGS
# --------------------------------------------------------

CFLAGS                :=    -ggdb3 -O3 -pthread -Wall
LDFLAGS               :=    -ggdb3 -pthread -O3
CPPFLAGS              :=    $(INCLUDE_DIRS) -DBUILD_DIR=\"$(BUILD_DIR_ABS)\"

# IMPORTANTE: Trace disabilitato per evitare errori di linker
CPPFLAGS              += -DTRACE_ON_ENTER=0 -DprojCOVERAGE_TEST=0

# --------------------------------------------------------
# BUILD RULES (uguale a prima)
# --------------------------------------------------------

OBJ_FILES = $(SOURCE_FILES:%.c=$(BUILD_DIR)/%.o)
DEP_FILE = $(OBJ_FILES:%.o=%.d)

${BIN} : $(BUILD_DIR)/$(BIN)

${BUILD_DIR}/${BIN} : ${OBJ_FILES}
	-mkdir -p ${@D}
	$(CC) $^ ${LDFLAGS} -o $@

-include ${DEP_FILE}

${BUILD_DIR}/%.o : %.c Makefile
	-mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -c $< -o $@

.PHONY: clean

clean:
	-rm -rf $(BUILD_DIR)