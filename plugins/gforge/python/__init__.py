from filly_graphical.backend import GuiBackend
from filly_graphical.widgets.hub import HubWindow
from filly_graphical.widgets.checklist import ChecklistWindow
from filly_graphical.widgets.input import InputWindow
from filly_graphical.widgets.msg import MsgWindow
from filly_graphical.widgets.yesno import YesNoWindow
from filly_graphical.widgets.menu import MenuWindow
from .stage3_picker import Stage3PickerWindow
from .profile_picker import ProfilePickerWindow
from .kernel_picker import KernelPickerWindow
from .use_flags import UseFlagsWindow
from .cflags import CflagsWindow

def register():
    GuiBackend.register_widget("gforge_hub", HubWindow)
    GuiBackend.register_widget("stage3_picker", Stage3PickerWindow)
    GuiBackend.register_widget("profile_picker", ProfilePickerWindow)
    GuiBackend.register_widget("kernel_picker", KernelPickerWindow)
    GuiBackend.register_widget("use_flags", UseFlagsWindow)
    GuiBackend.register_widget("cflags", CflagsWindow)
    GuiBackend.register_widget("portage_licenses", ChecklistWindow)
    GuiBackend.register_widget("portage_features", ChecklistWindow)
    GuiBackend.register_widget("portage_overlays", ChecklistWindow)
    GuiBackend.register_widget("portage_desktop_extras", ChecklistWindow)
    GuiBackend.register_widget("portage_tool_groups", ChecklistWindow)
    GuiBackend.register_widget("portage_mirrors", InputWindow)
    GuiBackend.register_widget("portage_binhost", InputWindow)
    GuiBackend.register_widget("portage_accept_keywords", MenuWindow)
    GuiBackend.register_widget("portage_video_cards", MenuWindow)
    GuiBackend.register_widget("portage_desktop_use", MsgWindow)
    GuiBackend.register_widget("portage_telemetry", YesNoWindow)