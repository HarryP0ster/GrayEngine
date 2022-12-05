﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows;
using System.Diagnostics;

enum SortType
{
    NAME_ASC = 0,
    NAME_DESC = 1
}

namespace EditorUI
{
    public partial class ModelBrowser : Window, EditorWindow
    {
        [DllImport("SceneEditor.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void LogMessage(IntPtr msg);
        [DllImport("user32.dll")]
        public static extern int SendMessage(IntPtr hWnd, int wMsg, IntPtr wParam, IntPtr lParam);
        [DllImport("user32.dll")]
        static extern IntPtr SetParent(IntPtr hWndChild, IntPtr hWndNewParent);
        [DllImport("user32.dll")]
        static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
        [DllImport("SceneEditor.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern void LoadModelFile(IntPtr mesh_path);
        [DllImport("SceneEditor.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern void AssignTextures(IntPtr image_path);
        [DllImport("SceneEditor.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern void LoadObject(IntPtr mesh_path, IntPtr tex_path);
        [DllImport("SceneEditor.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern void AddToTheScene(IntPtr mesh_path);
        [DllImport("SceneEditor.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern void CreateModelFile(IntPtr mesh_path, IntPtr textures);
        [DllImport("SceneEditor.dll", CallingConvention = CallingConvention.Cdecl)]
        static extern void CloseContext();

        IntPtr child_hwnd;
        bool b_fullpath = false;
        string loaded_mesh = "";
        string default_name = "";
        SortType sorting = SortType.NAME_ASC;
        System.Windows.Forms.Panel panel = new System.Windows.Forms.Panel();
        SortedDictionary<int, string> Materials = new SortedDictionary<int, string>();
        SortedDictionary<string, System.Windows.Controls.Control> LoadedAssets = new SortedDictionary<string, System.Windows.Controls.Control>();
        static string distr_location = System.IO.Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location);
        static string content_folder = distr_location + @"\Content\";
        static string missing_texture = System.IO.Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location) + @"\Content\Editor\MissingTexture.png";

        public ModelBrowser()
        {
            InitializeComponent();
            panel.CreateControl();
            panel.Dock = System.Windows.Forms.DockStyle.Fill;
            panel.BackColor = System.Drawing.Color.Black;
            panel.BorderStyle = System.Windows.Forms.BorderStyle.None;
            panel.Margin = new System.Windows.Forms.Padding(0);
            panel.BorderStyle = System.Windows.Forms.BorderStyle.None;
            FormHost.Child = panel;
            IdBox.AddHandler(System.Windows.Controls.TextBox.TextInputEvent, new System.Windows.Input.TextCompositionEventHandler(IdBox_TextInput), true);
            RefreshButton.Content = "\u21BA";
            MeshNameBar.Content = "Name \u2B9F";
            LoadData();
        }

        public void ParentRender(IntPtr child)
        {
            child_hwnd = child;
            SetParent(child, panel.Handle);
            UpdateChildPosition();
        }

        private void LoadData()
        {
            try
            {
                string[] models_files = System.IO.Directory.GetFiles(content_folder, "*.gmf", System.IO.SearchOption.AllDirectories);
                List<string> list_models = models_files.ToList();
                list_models.Sort();
                if (sorting == SortType.NAME_ASC)
                    list_models.Reverse();

                ClearBrowser();
                foreach (string model_file in list_models)
                {
                    if (model_file.ToLower().Contains(SearchBar.Text.ToLower()) || SearchBar.Text == "" || SearchBar.Text == "Search...")
                    {
                        System.IO.FileStream file = new System.IO.FileStream(model_file, System.IO.FileMode.Open);
                        byte[] byte_array = new byte[file.Length];
                        file.Read(byte_array, 0, byte_array.Length);
                        string oper = "";
                        bool writing = false;

                        foreach (char chr in byte_array)
                        {
                            if (chr == '<' && !writing)
                            {
                                writing = true;
                            }
                            else if (writing && chr != '>' && chr != '<')
                            {
                                oper += chr;
                            }
                            else if (chr == '>' && writing)
                            {
                                if (oper == "mesh")
                                {
                                    oper = "";
                                }
                                else
                                {
                                    writing = false;
                                    oper = "";
                                }
                            }
                            else if (chr == '<' && writing)
                            {
                                writing = false;
                                break;
                            }
                        }
                        file.Close();

                        BrowserItem item;
                        string[] name_cut = model_file.Split('\\');

                        if (!b_fullpath)
                        {

                            item = new BrowserItem("...\\" + name_cut[name_cut.Length - 2] + '\\' + name_cut[name_cut.Length - 1]);
                        }
                        else
                        {
                            item = new BrowserItem(model_file);
                        }

                        item.Padding = new System.Windows.Thickness(5);
                        item.ToolTip = model_file;
                        item.mesh_path = oper;

                        if (!LoadedAssets.ContainsKey(model_file))
                        {
                            LoadedAssets.Add(model_file, item);
                            Browser.Items.Add(item);
                            item.MainControl.AddHandler(System.Windows.Controls.Label.MouseDoubleClickEvent, new System.Windows.Input.MouseButtonEventHandler(BrowserItem_DoubleClick));
                        }
                    }
                }
            }
            catch (Exception e)
            {
                LogMessage(Marshal.StringToHGlobalAnsi("Exception occured in " + new StackFrame(1, true).GetMethod().Name + ": " + e.Message));
                CloseContext();
            }
        }

        internal void LoadDataAsync()
        {
            LoadData();
        }

        private void ClearBrowser()
        {
            if (Browser.Items.Count > 0)
            {
                Browser.SelectedIndex = -1;
                Browser.Items.Clear();
                LoadedAssets.Clear();
                return;
            }
        }

        public void UpdateChildPosition()
        {
            if (child_hwnd != null)
            {
                SetWindowPos(child_hwnd, IntPtr.Zero, 0, 0, panel.Width, panel.Height, 0);
            }
        }

        private void MdlBrowser_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            CloseContext();
        }

        private void BtnLoadMdl_Click(object sender, RoutedEventArgs e)
        {
            System.Windows.Forms.OpenFileDialog openFileDialog = new System.Windows.Forms.OpenFileDialog();
            openFileDialog.Filter = "obj files (*.obj)|*.obj";
            openFileDialog.InitialDirectory = content_folder;
            openFileDialog.RestoreDirectory = true;

            if (openFileDialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
            {
                Materials.Clear();
                loaded_mesh = openFileDialog.FileName;
                MeshPath.Text = loaded_mesh;
                MeshPath.ToolTip = loaded_mesh;
                default_name = openFileDialog.SafeFileName.Split('.')[0];
                IdBox.Text = default_name;

                LoadObject(Marshal.StringToHGlobalAnsi(openFileDialog.FileName), Marshal.StringToHGlobalAnsi(missing_texture + '|'));
            }

            GC.Collect();
        }

        private void MdlBrowser_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            UpdateChildPosition();
        }

        private void LoadMaterial(object sender, EventArgs e)
        {
            System.Windows.Forms.OpenFileDialog openFileDialog = new System.Windows.Forms.OpenFileDialog();
            openFileDialog.Filter = "png files (*.png)|*.png";
            openFileDialog.InitialDirectory = content_folder;
            openFileDialog.RestoreDirectory = true;

            if (openFileDialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
            {
                Materials[((MaterialInput)sender).material_index] = openFileDialog.FileName;
                ((MaterialInput)sender).MaterialPath.Text = openFileDialog.FileName;
                ((MaterialInput)sender).MaterialPath.ToolTip = openFileDialog.FileName;

                AssignTextures(Marshal.StringToHGlobalAnsi(GetTexturesString()));
            }

            GC.Collect();
        }

        internal void AddMaterialToTheTable(string material_names, string texture_names, int redraw)
        {
            MaterialsPanel.Children.Clear();

            int mat_index = 0;
            string[] materials = material_names.Split('|');
            string[] textures = texture_names.Split('|');

            foreach (string material_name in materials)
            {
                if (material_name != String.Empty)
                {
                    if (!Materials.ContainsKey(mat_index))
                    {
                        if (textures[mat_index] != "nil")
                            Materials.Add(mat_index, textures[mat_index]);
                        else
                        {
                            Materials.Add(mat_index, missing_texture);
                        }
                    }
                    else
                    {
                        if (textures[mat_index] != "nil")
                            Materials[mat_index] = textures[mat_index];
                        else
                        {
                            Materials[mat_index] = missing_texture;
                        }
                    }

                    MaterialInput material_panel = new MaterialInput();
                    material_panel.NameTextBlock.Text = material_name;
                    material_panel.MaterialPath.Text = Materials[mat_index];
                    material_panel.MaterialPath.ToolTip = Materials[mat_index];
                    material_panel.event_load_material += LoadMaterial;
                    material_panel.material_index = mat_index++;
                    MaterialsPanel.Children.Add(material_panel);
                    System.Windows.Controls.DockPanel.SetDock(material_panel, System.Windows.Controls.Dock.Top);
                }
            }
            if (redraw == 1)
                AssignTextures(Marshal.StringToHGlobalAnsi(GetTexturesString()));

            GC.Collect();
        }

        private string GetTexturesString()
        {
            string res = "";
            foreach (var tex in Materials)
            {
                res += tex.Value + '|';
            }

            return res;
        }

        private void BtnCreate_Click(object sender, RoutedEventArgs e)
        {
            string material_string = "";
            foreach (var texture in Materials.Values)
            {
                material_string += texture.Remove(0, distr_location.Length + 1) + "|";
            }

            CreateModelFile(Marshal.StringToHGlobalAnsi(loaded_mesh.Substring(0, loaded_mesh.LastIndexOf('\\')) + '\\' + IdBox.Text + ".gmf" + "|" + loaded_mesh.Remove(0, distr_location.Length + 1)), Marshal.StringToHGlobalAnsi(material_string));
            LoadData();
        }

        private void Browser_SelectionChanged(object sender, System.Windows.Controls.SelectionChangedEventArgs e)
        {
            try
            {
                if (Browser.SelectedIndex < 0) return;

                IdBox.Text = ((BrowserItem)Browser.SelectedItem).ToolTip.ToString().Split('\\').Last().Split('.')[0];
                loaded_mesh = ((BrowserItem)Browser.SelectedItem).ToolTip.ToString();
                MeshPath.Text = ((BrowserItem)Browser.SelectedItem).mesh_path;
                MeshPath.ToolTip = ((BrowserItem)Browser.SelectedItem).mesh_path;
                LoadModelFile(Marshal.StringToHGlobalAnsi(((BrowserItem)Browser.SelectedItem).ToolTip.ToString()));
                GC.Collect();
            }
            catch (Exception exc)
            {
                LogMessage(Marshal.StringToHGlobalAnsi("Exception occured in " + new StackFrame(1, true).GetMethod().Name + ": " + exc.Message));
                CloseContext();
            }
        }

        private void SearchBar_TextChanged(object sender, System.Windows.Controls.TextChangedEventArgs e)
        {
            if (SearchBar.IsFocused)
            {
                LoadData();
            }
        }

        private void SearchBar_GotFocus(object sender, RoutedEventArgs e)
        {
            if (SearchBar.Text == "Search...")
            {
                SearchBar.Text = "";
            }
        }

        private void SearchBar_LostFocus(object sender, RoutedEventArgs e)
        {
            if (SearchBar.Text == "")
            {
                SearchBar.Text = "Search...";
            }
        }

        private void FormHost_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            UpdateChildPosition();
        }

        private void GridSplitter_DragDelta(object sender, System.Windows.Controls.Primitives.DragDeltaEventArgs e)
        {
            TabCtr.Width += e.HorizontalChange;
        }

        private void MeshNameBar_Click(object sender, RoutedEventArgs e)
        {
            switch (sorting)
            {
                case SortType.NAME_ASC:
                    sorting = SortType.NAME_DESC;
                    MeshNameBar.Content = "Name \u2B9D";
                    break;
                case SortType.NAME_DESC:
                    sorting = SortType.NAME_ASC;
                    MeshNameBar.Content = "Name \u2B9F";
                    break;
                default:
                    sorting = SortType.NAME_ASC;
                    MeshNameBar.Content = "Name \u2B9F";
                    break;
            }

            LoadData();
        }

        private void ExpandBtn_Click(object sender, RoutedEventArgs e)
        {
            b_fullpath = !b_fullpath;

            ExpandBtn.Content = b_fullpath ? "-" : "+";
            ExpandBtn.ToolTip = b_fullpath ? "Condense path" : "Expand path";

            LoadData();
        }

        private void IdBox_TextInput(object sender, System.Windows.Input.TextCompositionEventArgs e)
        {
            string restriction = @"#%&{}\\<>*?/ $!':@+`|=." + '"';
            ((System.Windows.Controls.TextBox)sender).Undo();

            
            if (!restriction.Contains(e.Text))
            {
                ((System.Windows.Controls.TextBox)sender).Text += e.Text;
            }
        }

        private void TabCtr_SelectionChanged(object sender, System.Windows.Controls.SelectionChangedEventArgs e)
        {
            ((System.Windows.Controls.Control)sender).Focus();
        }

        private void IdBox_GotFocus(object sender, RoutedEventArgs e)
        {
            if (IdBox.Text == loaded_mesh.Split('\\').Last().Split('.')[0])
            {
                ((System.Windows.Controls.TextBox)sender).Text = String.Empty;
            }
        }

        private void IdBox_LostFocus(object sender, RoutedEventArgs e)
        {
            if (IdBox.Text == loaded_mesh.Split('\\').Last().Split('.')[0])
            {
                ((System.Windows.Controls.TextBox)sender).Text = default_name;
            }
        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
            LoadData();
        }

        private void BrowserItem_DoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            AddToTheScene(Marshal.StringToHGlobalAnsi(((BrowserItem)Browser.SelectedItem).ToolTip.ToString()));
            Close();
        }

        private void MdlBrowser_Closed(object sender, EventArgs e)
        {
            UIBridge.DestroyUserInterface(1);
        }
    }
}
