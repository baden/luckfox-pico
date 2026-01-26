# Rockchip sample Makefile
#LD_FLAGS += -L$(COMM_DIR)/lib -lsample_comm
#LD_FLAGS += $(RK_MEDIA_OPTS) -L$(RK_MEDIA_OUTPUT)/lib -lrockchip_mpp -lrkaiq -lrtsp
# ifeq ($(RK_MEDIA_CHIP), rv1106)
# LD_FLAGS += -lrockit_full
# else
# LD_FLAGS += -lrockit
# endif

all:
	@$(DRONE_CC) $(CFLAGS) drone.c -o drone $(LD_FLAGS)
install:
	@mv -f drone $(DRONE_OUT_DIR)/bin
# ifeq ($(HAVE_AVS), y)
# 	@mv -f sample_multi_vi_avs_osd_venc $(SAMPLE_OUT_DIR)/bin
# endif
clean:
	@echo "clean drone"
