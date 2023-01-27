$(call add-hdrs,fd_block.h)
$(call add-objs,fd_block,fd_ballet)
$(call make-unit-test,test_block,test_block,fd_ballet fd_util)
